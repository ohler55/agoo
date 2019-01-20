// Copyright (c) 2019, Peter Ohler, All rights reserved.

#ifndef AGOO_SECTIME_H
#define AGOO_SECTIME_H

#include <stdint.h>

typedef struct _agooTime {
    int sec;
    int min;
    int hour;
    int day;
    int mon;
    int year;
} *agooTime;

extern void	agoo_sectime(int64_t secs, agooTime at);

#endif /* AGOO_SECTIME_H */
