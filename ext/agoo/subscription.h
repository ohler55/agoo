// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SUBSCRIPTION_H__
#define __AGOO_SUBSCRIPTION_H__

#include <ruby.h>

struct _Upgraded;

typedef struct _Subscription {
    VALUE		self;
    struct _Upgraded	*up;
    uint64_t		id;
    VALUE		handler;
} *Subscription;

extern void	subscription_init(VALUE mod);
extern VALUE	subscription_new(struct _Upgraded *up, uint64_t id, VALUE handler);

#endif // __AGOO_SUBSCRIPTION_H__
