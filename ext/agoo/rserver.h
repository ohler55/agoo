// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RSERVER_H__
#define __AGOO_RSERVER_H__

#include <ruby.h>

#define MAX_WORKERS	32

typedef struct _RServer {
    int		worker_cnt;
    int		worker_pids[MAX_WORKERS];
    VALUE	*eval_threads; // Qnil terminated
} *RServer;

extern struct _RServer	the_rserver;

extern void	server_init(VALUE mod);
extern VALUE	rserver_shutdown(VALUE self);

#endif // __AGOO_RSERVER_H__
