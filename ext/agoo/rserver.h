// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RSERVER_H__
#define __AGOO_RSERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <ruby.h>

#include "log.h"
#include "queue.h"
#include "sub.h"
#include "upgraded.h"

#define MAX_WORKERS	32

typedef struct _RServer {
    // pub/sub (maybe common across servers,,,
    struct _Queue	pub_queue;
    struct _SubCache	sub_cache; // subscription cache

    struct _Queue	eval_queue;

    // upgrade
    pthread_mutex_t	up_lock;
    int			max_push_pending;
    Upgraded		up_list;

    // threads and workers
    int			worker_cnt;
    int			worker_pids[MAX_WORKERS];
    VALUE		*eval_threads; // Qnil terminated
} *RServer;

extern struct _RServer	the_rserver;

extern void	server_init(VALUE mod);
extern VALUE	rserver_shutdown(VALUE self);

#endif // __AGOO_RSERVER_H__
