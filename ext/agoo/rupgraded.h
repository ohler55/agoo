// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RUPGRADED_H
#define AGOO_RUPGRADED_H

#include <ruby.h>

#include "upgraded.h"

struct _agooCon;

extern void		upgraded_init(VALUE mod);
extern agooUpgraded	rupgraded_create(struct _agooCon *c, VALUE obj, VALUE env);

extern const char*	extract_subject(VALUE subject, int *slen);

#endif // AGOO_RUPGRADED_H
