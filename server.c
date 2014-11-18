#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct options {};

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: pik_server [OPTIONS]\n");
	printf("\n");
	printf("  A simple bandwidth tester.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
}

/** Prints version and copying information. */
static void version(void)
{
	printf("pik_server v. 0.1\n");
	printf("Copyright (C) 2014 IITiS PAN\n");
	printf("Author: Paweł Foremski <pjf@iitis.pl>\n");
}

/** Parses arguments
 * @retval 0     ok
 * @retval 1     error, main() should exit (eg. wrong arg. given)
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(struct options *opts, int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hv";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "help",       0, NULL,  1  },
		{ "version",    0, NULL,  2  },
		{ 0, 0, 0, 0 }
	};

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case 'h':
			case  1 : help(); return 2;
			case 'v':
			case  2 : version(); return 2;
			default: help(); return 1;
		}
	}

	return 0;
}

int main(int argc, char**argv)
{
	uint32_t b;
	int sockfd, i, j, n;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t len;
	char mesg[1000], buf[2000];
	char *cmd;

	if (parse_argv(NULL, argc, argv) != 0)
		return 1;

	/* prepare */

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(4000);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	bind(sockfd,(struct sockaddr *) &servaddr, sizeof servaddr);

	for (i = 0; i < sizeof mesg; i++) mesg[i] = random();

	/* handle clients */

	for (;;) {
		len = sizeof cliaddr;
		n = recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *) &cliaddr, &len);
		if (n <= 0) break;
		if (n < 7) continue;
		buf[n] = 0;

		// parse
		printf("Client %s: %s\n", inet_ntoa(cliaddr.sin_addr), buf);
		buf[5] = 0;
		cmd = buf;

		n = atoi(buf + 6);
		if (n < 1)
			n = 1;
		else if (n > 100000)
			n = 100000;

		if (strncmp(cmd, "PING ", 5) == 0) {
			if (n > sizeof mesg)
				n = sizeof mesg;

			printf("Pong %u\n", n);
			sendto(sockfd, mesg, n, 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);

		} else if (strncmp(cmd, "START", 5) == 0) {
			printf("Sending... ");

			b = 0;
			for (i = 0; i < n; i++) {
				j = sendto(sockfd, mesg, sizeof mesg, 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);
				if (j < 0) {
					perror("sendto");
					break;
				} else {
					b += j;
				}
			}

			printf("sent %u bytes in %u packets\n", b, i);
		}
	}

	return 0;
}
