/*
 * Copyright (C) 2014 IITiS PAN Gliwice <http://www.iitis.pl/>
 * Author: Paweł Foremski <pjf@iitis.pl>, 2014
 */

#ifndef _MEASURE_H_
#define _MEASURE_H_

#include <stdbool.h>

struct options {
	int n;
	int s;
	int t;
	int p;
	bool T;
	const char *ip;
};

struct result {
	int err;
	float latency;
	float bandwidth;
};

#define ERR_SUCCESS  0
#define ERR_EMPTY   -1
#define ERR_NO_DATA -2

/** Measure bandwidth to given host
 * @param opts   options */
struct result measure(struct options opts);

#endif
