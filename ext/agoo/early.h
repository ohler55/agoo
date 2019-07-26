// Copyright (c) 2019, Peter Ohler, All rights reserved.

#ifndef AGOO_EARLY_H
#define AGOO_EARLY_H

#include <stdbool.h>

#include "atomic.h"
#include "con.h"
#include "text.h"

struct _agooCon;

typedef struct _agooEarly {
    struct _agooEarly	*next;
    bool		sent;
    char		*link;
} *agooEarly;

extern agooEarly	agoo_early_create(const char *link);
extern void		agoo_early_destroy(agooEarly early);

#endif // AGOO_EARLY_H
