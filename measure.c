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

struct result measure(struct options opts)
{
	int sockfd, nfds, i;
	struct sockaddr_in servaddr;
	char buf[2000];
	fd_set fds;
	uint32_t diff;
	struct timeval tv_sel, tv_last, tv_first, T_first, T_now, T_last;
	uint32_t b;
	uint32_t rcv_n = 0, rcv_b = 0;
	uint32_t test_n = 0, test_b = 0;
	struct result res;

	res.err = ERR_EMPTY;

	bzero(&servaddr, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(opts.ip);
	servaddr.sin_port = htons(4000);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	nfds = sockfd + 1;

	/* ping - pong */

	for (i = 0; i < opts.p; i++) {
		printf("Ping... ");
		snprintf(buf, sizeof buf, "PING  50");
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &servaddr, sizeof servaddr);
		gettimeofday(&T_first, NULL);

		tv_sel.tv_sec = 1;
		tv_sel.tv_usec = 0;
		if (select(nfds, &fds, NULL, NULL, &tv_sel) > 0) {
			recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
			ioctl(sockfd, SIOCGSTAMP, &T_last);
			res.latency = 0.001 * timediff(&T_last, &T_first);
			printf("pong (%.1f ms)\n", res.latency);

			if (i == opts.p - 1)
				break;
			else
				sleep(1);
		} else {
			printf("fail\n");
		}
	}

	/* request and receive */

	snprintf(buf, sizeof buf, "START %d", opts.n);
	printf("Sending '%s' to %s\n", buf, opts.ip);
	sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &servaddr, sizeof servaddr);

	tv_sel.tv_sec = opts.t;
	tv_sel.tv_usec = 0;

	while (select(nfds, &fds, NULL, NULL, &tv_sel) > 0) {
		b = recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
		rcv_b += b;
		rcv_n++;

		if (rcv_n == 1) {
			printf("Receiving... will skip first %uKB\n", opts.s);
		}

		// skipped enough data?
		if (rcv_n > opts.s) {
			test_b += b;
			test_n++;
			if (test_n == 1)
				ioctl(sockfd, SIOCGSTAMP, &tv_first);
		}

		// print receive timings?
		if (opts.T) {
			if (rcv_n == 1) {
				ioctl(sockfd, SIOCGSTAMP, &T_first);
				T_last = T_first;
				printf("1\t0\t%u\t0\n", rcv_b);
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
