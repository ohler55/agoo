// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include "pusher.h"

static void
pusher_mark(void *ptr) {
    if (NULL != ptr) {
	Pusher		p = (Pusher)ptr;
	PusherSlot	ps;
	PusherSlot	*psp;
	PusherSlot	*end = psp + PUSHER_BUCKET_SIZE;

	if (Qnil != p->wrap) {
	    rb_gc_mark(p->wrap);
	}
	for (psp = p->buckets; psp < end; psp++) {
	    for (ps = *psp; NULL != ps; ps = ps->next) {
		rb_gc_mark(ps->handler);
	    }
	}
    }
}

void
pusher_init(Pusher p) {
    memset(p, 0, sizeof(struct _Pusher));
    p->wrap = Data_Wrap_Struct(rb_cObject, pusher_mark, NULL, p);
}

void
pusher_cleanup(Pusher p) {
    PusherSlot	ps;
    PusherSlot	head;
    PusherSlot	*psp;
    int		i;
    
    if (Qnil != p->wrap) {
	DATA_PTR(p->wrap) = NULL;
    }
    for (i = 0, psp = p->buckets; i < PUSHER_BUCKET_SIZE; i++, psp++) {
	head = *psp;
	while (NULL != head) {
	    ps = head;
	    head = head->next;
	    xfree(ps);
	}
    }
}

void
pusher_add(Pusher p, uint64_t id, VALUE handler) {
    PusherSlot	*bucket = p->buckets + (PUSHER_BUCKET_MASK & id);
    PusherSlot	ps = ALLOC(struct _PusherSlot);

    ps->key = id;
    ps->handler = handler;
    ps->next = *bucket;
    *bucket = ps;
}

VALUE
pusher_get(Pusher p, uint64_t id) {
    PusherSlot	ps = *(p->buckets + (PUSHER_BUCKET_MASK & id));

    for (; NULL != ps; ps = ps->next) {
	if (ps->key == id) {
	    return ps->handler;
	}
    }
    return Qnil;
}

void
pusher_del(Pusher p, uint64_t id) {
    PusherSlot	*bucket = p->buckets + (PUSHER_BUCKET_MASK & id);

    if (NULL != *bucket) {
	PusherSlot	ps = *bucket;

	if (ps->key == id) {
	    *bucket = ps->next;
	} else {
	    PusherSlot	prev = ps;
	    
	    for (ps = ps->next; NULL != ps; ps = ps->next) {
		if (ps->key == id) {
		    prev->next = ps->next;
		    xfree(ps);
		    break;
		}
		prev = ps;
	    }
	}
    }
}

