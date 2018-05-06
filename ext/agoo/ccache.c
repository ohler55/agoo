// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ccache.h"
#include "con.h"
#include "debug.h"

static void
cc_mark(void *ptr) {
    if (NULL != ptr) {
	CCache	cc = (CCache)ptr;
	CSlot	slot;
	CSlot	*bp = cc->buckets;
	CSlot	*end = bp + CC_BUCKET_SIZE;

	if (Qnil != cc->wrap) {
	    rb_gc_mark(cc->wrap);
	}
	pthread_mutex_lock(&cc->lock);
	for (bp = cc->buckets; bp < end; bp++) {
	    for (slot = *bp; NULL != slot; slot = slot->next) {
		if (Qnil != slot->handler) {
		    rb_gc_mark(slot->handler);
		}
	    }
	}
	pthread_mutex_unlock(&cc->lock);
    }
}


void
cc_init(CCache cc) {
    memset(cc, 0, sizeof(struct _CCache));
    pthread_mutex_init(&cc->lock, 0);
    cc->wrap = Data_Wrap_Struct(rb_cObject, cc_mark, NULL, cc);
}

void
cc_cleanup(CCache cc) {
    CSlot	*sp = cc->buckets;
    CSlot	s;
    CSlot	n;
    int		i;
    
    for (i = CC_BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    DEBUG_FREE(mem_cslot, s);
	    free(s);
	}
	*sp = NULL;
    }
}

CSlot
cc_set_con(CCache cc, Con con) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & con->id);
    CSlot	slot;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (con->id == slot->cid) {
	    slot->con = con;
	    break;
	}
    }
    if (NULL == slot) {
	if (NULL != (slot = (CSlot)malloc(sizeof(struct _CSlot)))) {
	    DEBUG_ALLOC(mem_cslot, slot);
	    slot->cid = con->id;
	    slot->con = con;
	    slot->handler = Qnil;
	    slot->on_empty = false;
	    slot->on_close = false;
	    slot->on_shut = false;
	    slot->on_msg = false;
	    atomic_init(&slot->pending, 0);
	    atomic_init(&slot->ref_cnt, 1);
	    slot->next = *bucket;
	    *bucket = slot;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return slot;
}

void
cc_set_handler(CCache cc, uint64_t cid, VALUE handler, bool on_empty, bool on_close, bool on_shut, bool on_msg) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    slot->handler = handler;
	    break;
	}
    }
    if (NULL == slot) {
	// TBD is is faster to lock twice or allocate while locked?
	if (NULL != (slot = (CSlot)malloc(sizeof(struct _CSlot)))) {
	    DEBUG_ALLOC(mem_cslot, slot);
	    slot->cid = cid;
	    slot->con = NULL;
	    slot->handler = handler;
	    slot->on_empty = on_empty;
	    slot->on_close = on_close;
	    slot->on_shut = on_shut;
	    slot->on_msg = on_msg;
	    atomic_init(&slot->pending, 0);
	    atomic_init(&slot->ref_cnt, 1);
	    slot->next = *bucket;
	    slot->on_empty = on_empty;
	    slot->on_close = on_close;
	    slot->on_shut = on_shut;
	    slot->on_msg = on_msg;
	    *bucket = slot;
	}
    }
    pthread_mutex_unlock(&cc->lock);
}

void
cc_remove(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    CSlot	prev = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    if (NULL == prev) {
		*bucket = slot->next;
	    } else {
		prev->next = slot->next;
	    }
	    break;
	}
	prev = slot;
    }
    pthread_mutex_unlock(&cc->lock);
    if (NULL != slot) {
	DEBUG_FREE(mem_cslot, slot)
	free(slot);
    }
}

void
cc_remove_con(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    CSlot	prev = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    if (atomic_fetch_sub(&slot->ref_cnt, 1) <= 1) {
		if (NULL == prev) {
		    *bucket = slot->next;
		} else {
		    prev->next = slot->next;
		}
	    } else {
		atomic_store(&slot->pending, -1);
		slot = NULL;
	    }
	    break;
	}
	prev = slot;
    }
    pthread_mutex_unlock(&cc->lock);
    if (NULL != slot) {
	DEBUG_FREE(mem_cslot, slot)
	free(slot);
    }
}

VALUE
cc_ref_dec(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    VALUE	handler = Qnil;
    CSlot	prev = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    int	rcnt = atomic_fetch_sub(&slot->ref_cnt, 1);

	    handler = slot->handler;
	    if (rcnt <= 1) {
		if (NULL == prev) {
		    *bucket = slot->next;
		} else {
		    prev->next = slot->next;
		}
	    } else {
		slot = NULL;
	    }
	    break;
	}
	prev = slot;
    }
    pthread_mutex_unlock(&cc->lock);
    if (NULL != slot) {
	DEBUG_FREE(mem_cslot, slot)
	free(slot);
    }
    return handler;
}

Con
cc_get_con(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    Con		con = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    con = slot->con;
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return con;
}

VALUE
cc_get_handler(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    VALUE	handler = Qnil;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    handler = slot->handler;
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return handler;
}

int
cc_get_pending(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    int		pending = -1;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    pending = atomic_load(&slot->pending);
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return pending;
}

CSlot
cc_get_slot(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return slot;
}

void
cc_pending_inc(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid == slot->cid) {
	    atomic_fetch_add(&slot->pending, 1);
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);
}
