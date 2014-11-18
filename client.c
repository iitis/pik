/*
 * Copyright (C) 2014 IITiS PAN Gliwice <http://www.iitis.pl/>
 * Author: Paweł Foremski <pjf@iitis.pl>, 2014
 */

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lib.h"
#include "measure.h"

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: pik_client [OPTIONS] <IP>\n");
	printf("\n");
	printf("  A simple bandwidth tester.\n");
	printf("\n");
	printf("Options:\n");
	printf("  -n <kbytes>            amount of kilobytes to use [50]\n");
	printf("  -s <kbytes>            amount of kilobytes to skip [10]\n");
	printf("  -T                     print receive timings\n");
	printf("  -t <seconds>           time limit [1]\n");
	printf("  -p <num>               number of pings to send [1]\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
}

/** Prints version and copying information. */
static void version(void)
{
	printf("pik_client v. 0.1\n");
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

	static char *short_opts = "hvn:s:Tt:p:";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "help",       0, NULL,  1  },
		{ "version",    0, NULL,  2  },
		{ 0, 0, 0, 0 }
	};

	/* defaults */
	opts->n = 50;
	opts->s = 10;
	opts->T = false;
	opts->t = 1;
	opts->p = 1;

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case 'h':
			case  1 : help(); return 2;
			case 'v':
			case  2 : version(); return 2;
			case 'n': opts->n = atoi(optarg); break;
			case 's': opts->s = atoi(optarg); break;
			case 't': opts->t = atoi(optarg); break;
			case 'p': opts->p = atoi(optarg); break;
			case 'T': opts->T = true; break;
			default: help(); return 1;
		}
	}

	if (opts->n <= 9)
		opts->n = 10;

	if (opts->s <= 0)
		opts->s = 0;
	else if (opts->s > opts->n)
		opts->s = opts->n / 2;

	if (opts->t <= 0)
		opts->t = 1;

	if (opts->p < 0)
		opts->p = 0;

	if (argc - optind > 0) {
		opts->ip = (const char *) argv[optind];
	} else {
		help();
		return 1;
	}

	return 0;
}

int main(int argc, char**argv)
{
	struct options opts;
	struct result res;

	if (parse_argv(&opts, argc, argv) != 0)
		return 1;

	res = measure(opts);
	switch (res.err) {
		case ERR_EMPTY:
			printf("No result");
			return 2;
		case ERR_NO_DATA:
			printf("Not enough data received");
			return 3;
		default:
			break;
	}

	printf("Bandwidth: %g kbit/s\n", res.bandwidth);
	printf("Latency: %g ms\n", res.latency);

	return 0;
}
