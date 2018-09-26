// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RUPGRADED_H__
#define __AGOO_RUPGRADED_H__

#include <ruby.h>

#include "upgraded.h"

struct _Con;

extern void	upgraded_init(VALUE mod);
extern Upgraded	rupgraded_create(struct _Con *c, VALUE obj, VALUE env);

extern const char*	extract_subject(VALUE subject, int *slen);

#endif // __AGOO_RUPGRADED_H__
