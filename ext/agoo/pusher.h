// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_PUSHER_H__
#define __AGOO_PUSHER_H__

#include <ruby.h>

#define PUSHER_BUCKET_SIZE	1024
#define PUSHER_BUCKET_MASK	1023

typedef struct _PusherSlot {
    struct _PusherSlot	*next;
    uint64_t		key;
    VALUE		handler;
} *PusherSlot;

typedef struct _Pusher {
    VALUE		wrap;
    PusherSlot		buckets[PUSHER_BUCKET_SIZE];
} *Pusher;

extern void	pusher_init(Pusher p);
extern void	pusher_cleanup(Pusher p);
extern void	pusher_add(Pusher p, uint64_t id, VALUE handler);
extern VALUE	pusher_get(Pusher p, uint64_t id);
extern void	pusher_del(Pusher p, uint64_t id);

#endif // __AGOO_PUSHER_H__
