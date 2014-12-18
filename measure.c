/*
 * Copyright (C) 2014 IITiS PAN Gliwice <http://www.iitis.pl/>
 * Author: Paweł Foremski <pjf@iitis.pl>, 2014
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>

#include "lib.h"
#include "measure.h"

/** Time difference in microseconds
 * @param a   now (present)
 * @param b   reference (past)
 */
static uint32_t timediff(struct timeval *a, struct timeval *b)
{
	if (a->tv_sec > b->tv_sec)
		return (a->tv_sec  - b->tv_sec) * 1000000 - b->tv_usec + a->tv_usec;
	else if (a->tv_sec == b->tv_sec && a->tv_usec > b->tv_usec)
		return a->tv_usec - b->tv_usec;
	else
		return 0;
}

static bool wait_fd(int fd, int t)
{
	fd_set fds;
	struct timeval tv_sel;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv_sel.tv_sec = 1;
	tv_sel.tv_usec = 0;

	return select(fd+1, &fds, NULL, NULL, &tv_sel) > 0;
}

static bool wait_fd2(int fd, struct timeval *tv_sel)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	return select(fd+1, &fds, NULL, NULL, tv_sel) > 0;
}

/****************************************************/


struct result pik(struct client_options opts)
{
	int sockfd, i, n;
	struct sockaddr_in servaddr;
	char buf[2000], pwd[16] = {0};
	uint32_t diff;
	struct timeval tv_sel, tv_last, tv_first, T_first, T_now, T_last, T_start, T_ref;
	uint32_t b;
	uint32_t rcv_n = 0, rcv_b = 0;
	uint32_t test_n = 0, test_b = 0;
	struct result res;
	struct hostent *dns;
	float lat;

	res.err = ERR_EMPTY;
	res.bandwidth = -1.0;
	res.latency = -1.0;

	/* resolve DNS */

	dns = gethostbyname(opts.dst);
	if (!dns) {
		printf("Unknown host: %s\n", opts.dst);
		res.err = ERR_UNKHOST;
		return res;
	}

	/* connect */

	bzero(&servaddr, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	bcopy(dns->h_addr, &(servaddr.sin_addr.s_addr), dns->h_length);
	servaddr.sin_port = htons(opts.port);

	printf("Connecting to %s (%s) port %u\n", opts.dst, inet_ntoa(servaddr.sin_addr), opts.port);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	/* register */

	for (i = 0; i < 3; i++) {
		printf("Register... ");
		snprintf(buf, sizeof buf, "REGISTER");
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &servaddr, sizeof servaddr);

		if (wait_fd(sockfd, 1)) {
			n = recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
			if (n <= 0) continue;

			/* received password */
			buf[n] = 0;
			strncpy(pwd, buf, sizeof pwd);
			pwd[(sizeof pwd)-1] = 0;
			printf("%s\n", pwd);
			break;
		} else {
			printf("fail (timeout)\n");
		}
	}

	if (!pwd[0]) {
		printf("Registration failed\n");
		res.err = ERR_REGISTER;
		return res;
	}

	/* ping - pong */

	for (i = 0; i < opts.ping; i++) {
		printf("Ping... ");
		snprintf(buf, sizeof buf, "PING 50 %s", pwd);
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &servaddr, sizeof servaddr);
		gettimeofday(&T_first, NULL);

		if (wait_fd(sockfd, 3)) {
			recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
			ioctl(sockfd, SIOCGSTAMP, &T_last);
			lat = 0.001 * timediff(&T_last, &T_first);
			printf("pong (%.1f ms)\n", lat);

			if (res.latency < 0)
				res.latency = lat;
			else
				res.latency = (res.latency + lat) / 2.0;

			if (i == opts.ping - 1)
				break;
			else
				sleep(1);
		} else {
			printf("fail\n");
		}
	}

	/* request and receive */

	snprintf(buf, sizeof buf, "START %d %s", opts.n, pwd);
	printf("Sending '%s' to %s\n", buf, opts.dst);
	sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &servaddr, sizeof servaddr);
	gettimeofday(&T_start, NULL);

	// expected time of first received packet
	if (res.latency > 0) {
		diff = res.latency * 1000.0 * 0.8;
		T_last.tv_sec = diff / 1000000;
		T_last.tv_usec = diff % 1000000;
		timeradd(&T_start, &T_last, &T_ref);
	}

	tv_sel.tv_sec = opts.t;
	tv_sel.tv_usec = 0;
	while (wait_fd2(sockfd, &tv_sel)) {
		b = recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
		if (b < 100) continue;

		rcv_b += b;
		rcv_n++;

		if (rcv_n == 1) {
			printf("Receiving... will skip first %uKB\n", opts.s);
		}

		// skipped enough data?
		if (rcv_n > opts.s) {
			test_b += b;
			test_n++;

			// first received packet?
			if (test_n == 1) {
				ioctl(sockfd, SIOCGSTAMP, &tv_first);
				if (opts.s == 0 && timercmp(&tv_first, &T_ref, >)) {
					printf("Corrected tv_first\n");
					tv_first = T_ref;
				}
			}
		}

		// print receive timings?
		if (opts.T) {
			if (rcv_n == 1) {
				ioctl(sockfd, SIOCGSTAMP, &T_first);
				diff = timediff(&T_first, &T_start);
				T_last = T_first;
				printf("1\t0\t%u\t%u\n", rcv_b, diff);
			} else {
				ioctl(sockfd, SIOCGSTAMP, &T_now);
				diff = timediff(&T_now, &T_last);

				printf("%u\t%u\t%u\t%u\n", rcv_n, timediff(&T_now, &T_first), rcv_b, diff);
				T_last = T_now;
			}
		}

		// finished?
		if (rcv_n >= opts.n) break;
	}
	ioctl(sockfd, SIOCGSTAMP, &tv_last);

	/* valid? */

	if (opts.n - opts.s > 10 && test_n < 10) {
		printf("Not enough data received, try -T option to analyze\n");
		res.err = ERR_NO_DATA;
		return res;
	}

	/* calculate */

	diff = timediff(&tv_last, &tv_first);
	printf("Received %u bytes in %d packets in %u us\n", test_b, test_n, diff);

	res.bandwidth = test_b * 8000. / diff;
	res.err = ERR_SUCCESS;
	return res;
}
