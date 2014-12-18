/*
 * Copyright (C) 2014 IITiS PAN Gliwice <http://www.iitis.pl/>
 * Author: Paweł Foremski <pjf@iitis.pl>, 2014
 */

#ifndef _MEASURE_H_
#define _MEASURE_H_

#include <stdbool.h>

#define PIK_VERSION "0.3"

struct client_options {
	int n;            // total KB to request
	int s;            // skip first s KB
	int t;            // time limit for receiving
	int ping;         // number of pings
	int port;         // UDP port number
	bool T;           // print timestampped data?
	const char *dst;  // destination
};

struct server_options {
	int port;         // UDP port number
	float max_rate;   // max send rate
};

struct result {
	int err;
	float latency;
	float bandwidth;
};

#define ERR_SUCCESS   0
#define ERR_EMPTY    -1
#define ERR_NO_DATA  -2
#define ERR_UNKHOST  -3
#define ERR_REGISTER -4

/** Measure bandwidth using pik
 * @param opts   options */
struct result pik(struct client_options opts);

#endif
