#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libpjf/main.h>

#include "measure.h"

struct dbentry {
	uint32_t timestamp;
	uint32_t random;
	char pwd[16];
	uint32_t last_register;
	uint32_t last_start;
};

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: pik_server [OPTIONS]\n");
	printf("\n");
	printf("  A simple bandwidth tester.\n");
	printf("\n");
	printf("Options:\n");
	printf("  -p <num>               set port number [4000]\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
	printf("  --debug,-d <num>       set debugging level\n");
}

/** Prints version and copying information. */
static void version(void)
{
	printf("pik_server v. " PIK_VERSION "\n");
	printf("Copyright (C) 2014 IITiS PAN\n");
	printf("Author: Paweł Foremski <pjf@iitis.pl>\n");
}

/** Parses arguments
 * @retval 0     ok
 * @retval 1     error, main() should exit (eg. wrong arg. given)
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(struct server_options *opts, int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hvd:p:";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "help",       0, NULL,  1  },
		{ "version",    0, NULL,  2  },
		{ "debug",      1, NULL,  3  },
		{ 0, 0, 0, 0 }
	};

	/* defaults */
	opts->port = 4000;

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case 'h':
			case  1 : help(); return 2;
			case 'v':
			case  2 : version(); return 2;
			case  3 : debug = atoi(optarg); break;
			case 'p': opts->port = atoi(optarg); break;
			default: help(); return 1;
		}
	}

	if (opts->port < 1 || opts->port >65535)
		opts->port = 4000;

	return 0;
}

static void db_clear(void *ptr)
{
	mmatic_free(ptr);
}

static struct dbentry *db_get(thash *db, const char *client)
{
	struct dbentry *e;
	uint32_t now = pjf_timestamp();

	/* hacky but works ;) */
	if (thash_count(db) > 100000)
		thash_flush(db);

	e = thash_get(db, client);
	if (!e || now - e->timestamp > 60) {
		if (!e) {
			e = mmatic_zalloc(db, sizeof *e);
			thash_set(db, client, e);
		}

		e->random = (uint32_t) random();
		e->timestamp = pjf_timestamp();
		snprintf(e->pwd, sizeof(e->pwd), "%u", e->random);
		dbg(3, "Generated new password for %s: %s\n", client, e->pwd);
	}

	return e;
}


/*************************************************************/

int main(int argc, char**argv)
{
	struct server_options opts;
	int sockfd, i, j, n;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t len;
	char mesg[1000], buf[2000];
	char *client, *cmd, *arg, *pwd;
	struct dbentry *e;
	mmatic *mm;
	thash *db;

	if (parse_argv(&opts, argc, argv) != 0)
		return 1;

	/* prepare */

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(opts.port);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(sockfd,(struct sockaddr *) &servaddr, sizeof servaddr) < 0) {
		perror("bind");
		return 1;
	}

	dbg(1, "pik_server listening on port %u\n", opts.port);

	srand(pjf_timestamp());
	for (i = 0; i < sizeof mesg; i++) mesg[i] = random();

	mm = mmatic_create();
	db = thash_create_strkey(db_clear, mm);

	/* handle clients */

	for (;;) {
		len = sizeof cliaddr;
		n = recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *) &cliaddr, &len);
		if (n < 0) break;
		if (n < 6) continue;
		buf[n] = 0;

		// parse
		client = inet_ntoa(cliaddr.sin_addr);
		dbg(5, "client %s: %s\n", client, buf);
		cmd = strtok(buf, " ");
		arg = strtok(NULL, " ");
		pwd = strtok(NULL, " ");

		// number of bytes
		n = arg ? atoi(arg) : 1;
		if (n < 1) n = 1;
		else if (n > 10000) n = 10000;

		// client info
		e = db_get(db, client);

		if (streq(cmd, "REGISTER")) {
			if (pjf_timestamp() - e->last_register < 1) continue;

			dbg(2, "REGISTER from %s\n", client);
			e->last_register = pjf_timestamp();

			sendto(sockfd, e->pwd, sizeof(e->pwd), 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);

		} else if (streq(cmd, "PING")) {
			if (!pwd || !streq(pwd, e->pwd)) continue;
			if (n > sizeof mesg) n = sizeof mesg;

			dbg(2, "PING %u from %s\n", n, client);
			sendto(sockfd, mesg, n, 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);

		} else if (streq(cmd, "START")) {
			if (!pwd || !streq(pwd, e->pwd)) continue;
			if (pjf_timestamp() - e->last_start < 1) continue;
			if (n > 1000) n = 1000;

			dbg(2, "START %u from %s\n", n, client);
			e->last_start = pjf_timestamp();

			for (i = 0; i < n; i++) {
				j = sendto(sockfd, mesg, sizeof mesg, 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);
				if (j < 0) break;
			}
		}
	}

	return 0;
}
