// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ccache.h"
#include "con.h"
#include "debug.h"

void
cc_init(CCache cc) {
    memset(cc, 0, sizeof(struct _CCache));
    pthread_mutex_init(&cc->lock, 0);
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
	    DEBUG_FREE(mem_cslot)
	    free(s);
	}
	*sp = NULL;
    }
}

CSlot
cc_set_con(CCache cc, uint64_t cid, Con con) {
    CSlot	slot = (CSlot)malloc(sizeof(struct _CSlot));
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);

    if (NULL != slot) {
	slot->cid = con->id;
	slot->con = con;
	slot->handler = Qnil;
	atomic_init(&slot->pending, 0);
	atomic_init(&slot->ref_cnt, 1);
	pthread_mutex_lock(&cc->lock);
	slot->next = *bucket;
	*bucket = slot;
	pthread_mutex_unlock(&cc->lock);
    }
    return slot;
}

void
cc_remove(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    CSlot	prev = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid = slot->cid) {
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
	free(slot);
    }
}

void
cc_set_handler(CCache cc, uint64_t cid, VALUE handler) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid = slot->cid) {
	    slot->handler = handler;
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);
}

Con
cc_get_con(CCache cc, uint64_t cid) {
    CSlot	*bucket = cc->buckets + (CC_BUCKET_MASK & cid);
    CSlot	slot;
    Con		con = NULL;

    pthread_mutex_lock(&cc->lock);
    for (slot = *bucket; NULL != slot; slot = slot->next) {
	if (cid = slot->cid) {
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
	if (cid = slot->cid) {
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
	if (cid = slot->cid) {
	    pending = slot->pending;
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
	if (cid = slot->cid) {
	    break;
	}
    }
    pthread_mutex_unlock(&cc->lock);

    return slot;
}
