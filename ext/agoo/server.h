// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SERVER_H__
#define __AGOO_SERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <ruby.h>

#include "hook.h"
#include "log.h"
#include "page.h"
#include "queue.h"

typedef struct _Server {
    volatile bool	active;
    volatile bool	ready;
    int			thread_cnt;
    int			port;
    bool		pedantic;
    char		*root;
    atomic_int		running;
    pthread_t		listen_thread;
    pthread_t		con_thread;
    struct _Log		log;
    struct _LogCat	error_cat;
    struct _LogCat	warn_cat;
    struct _LogCat	info_cat;
    struct _LogCat	debug_cat;
    struct _LogCat	con_cat;
    struct _LogCat	req_cat;
    struct _LogCat	resp_cat;
    struct _LogCat	eval_cat;
    
    struct _Queue	con_queue;
    struct _Cache	pages;

    Hook		hooks;
    struct _Queue	eval_queue;

    VALUE		*eval_threads; // Qnil terminated
} *Server;

extern void	server_init(VALUE mod);

#endif // __AGOO_SERVER_H__
