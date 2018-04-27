// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_CCACHE_H__
#define __AGOO_CCACHE_H__

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include <ruby.h>

#define CC_BUCKET_SIZE	1024
#define CC_BUCKET_MASK	1023

struct _Con;

typedef struct _CSlot {
    struct _CSlot	*next;
    uint64_t		cid;
    struct _Con		*con;
    VALUE		handler;
    atomic_int		pending;
    atomic_int		ref_cnt;
    bool		on_empty;
    bool		on_close;
    bool		on_shut;
    bool		on_msg;
} *CSlot;

typedef struct _CCache {
    CSlot		buckets[CC_BUCKET_SIZE];
    pthread_mutex_t	lock;
    VALUE		wrap;
} *CCache;

extern void		cc_init(CCache cc);
extern void		cc_cleanup(CCache cc);
extern CSlot		cc_set_con(CCache cc, struct _Con *con);
extern void		cc_set_handler(CCache cc, uint64_t cid, VALUE handler, bool on_empty, bool on_close, bool on_shut, bool on_msg);
extern void		cc_remove(CCache cc, uint64_t cid);

extern void		cc_remove_con(CCache cc, uint64_t cid);
extern VALUE		cc_ref_dec(CCache cc, uint64_t cid);

extern struct _Con*	cc_get_con(CCache cc, uint64_t cid);
extern VALUE		cc_get_handler(CCache cc, uint64_t cid);
extern int		cc_get_pending(CCache cc, uint64_t cid);
extern CSlot		cc_get_slot(CCache cc, uint64_t cid);

extern void		cc_pending_inc(CCache cc, uint64_t cid);

#endif /* __AGOO_CCACHE_H__ */
