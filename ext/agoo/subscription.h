// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SUBSCRIPTION_H__
#define __AGOO_SUBSCRIPTION_H__

#include <ruby.h>

struct _Server;

typedef struct _Subscription {
    struct _Server	*server;
    VALUE		self;
    uint64_t		cid;
    uint64_t		id;
    VALUE		handler;
} *Subscription;

extern void	subscription_init(VALUE mod);
extern VALUE	subscription_new(struct _Server *s, uint64_t cid, uint64_t id, VALUE handler);

#endif // __AGOO_SUBSCRIPTION_H__
