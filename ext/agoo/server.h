// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SERVER_H__
#define __AGOO_SERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <ruby.h>

#include "rhook.h"
#include "log.h"
#include "page.h"
#include "queue.h"
#include "sub.h"
#include "upgraded.h"

#define MAX_WORKERS	32

typedef struct _Server {
    volatile bool	inited;
    volatile bool	active;
    int			thread_cnt;
    int			worker_cnt;
    int			max_push_pending;
    int			port;
    int			fd;
    bool		pedantic;
    bool		root_first;
    atomic_int		running;
    pthread_t		listen_thread;
    pthread_t		con_thread;

    pthread_mutex_t	up_lock;
    Upgraded		up_list;

    struct _Queue	con_queue;
    struct _Queue	pub_queue;
    struct _Cache	pages;
    struct _SubCache	sub_cache; // subscription cache

    Hook		hooks;
    Hook		hook404;
    struct _Queue	eval_queue;

    int			worker_pids[MAX_WORKERS];
    VALUE		*eval_threads; // Qnil terminated
} *Server;

extern struct _Server	the_server;

extern void	server_init(VALUE mod);
extern void	server_shutdown();

#endif // __AGOO_SERVER_H__
