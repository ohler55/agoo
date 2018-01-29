// Copyright 2009, 2015, 2016, 2018 by Peter Ohler, All Rights Reserved

#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "dtime.h"

#define MIN_SLEEP	(1.0 / (double)CLOCKS_PER_SEC)

double
dtime() {
    struct timeval	tv;
    struct timezone	tz;

    gettimeofday(&tv, &tz);

    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

double
dsleep(double t) {
    struct timespec	req, rem;

    if (MIN_SLEEP > t) {
	t = MIN_SLEEP;
    }
    req.tv_sec = (time_t)t;
    req.tv_nsec = (long)(1000000000.0 * (t - (double)req.tv_sec));
    if (nanosleep(&req, &rem) == -1 && EINTR == errno) {
	return (double)rem.tv_sec + (double)rem.tv_nsec / 1000000000.0;
    }
    return 0.0;
}

double
dwait(double t) {
    double	end = dtime() + t;
    
    if (MIN_SLEEP < t) {
	struct timespec	req, rem;

	t -= MIN_SLEEP;
	req.tv_sec = (time_t)t;
	req.tv_nsec = (long)(1000000000.0 * (t - (double)req.tv_sec));
	nanosleep(&req, &rem);
    }
    while (dtime() < end) {
	continue;
    }
    return 0.0;
}
