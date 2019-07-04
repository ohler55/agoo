// Copyright (c) 2019, Peter Ohler, All rights reserved.

#ifndef AGOO_EARLY_HINTS_H
#define AGOO_EARLY_HINTS_H

#include <ruby.h>

struct _agooReq;

extern void	early_hints_init(VALUE mod);
extern VALUE	agoo_early_hints_new(struct _agooReq *req);

#endif // AGOO_EARLY_HINTS_H
