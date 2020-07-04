// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RSERVER_H
#define AGOO_RSERVER_H

#include <ruby.h>

#define MAX_WORKERS	32

typedef struct _rUse {
    struct _rUse	*next;
    VALUE		clas;
    VALUE		*argv;
    int			argc;
} *RUse;

typedef struct _rServer {
    int		worker_cnt;
    int		worker_pids[MAX_WORKERS];
    VALUE	*eval_threads; // Qnil terminated
    RUse	uses;
} *RServer;

extern struct _rServer	the_rserver;

extern void	server_init(VALUE mod);
extern VALUE	rserver_shutdown(VALUE self);

#endif // AGOO_RSERVER_H
