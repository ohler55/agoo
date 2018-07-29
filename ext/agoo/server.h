// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SERVER_H__
#define __AGOO_SERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "bind.h"
#include "err.h"
#include "hook.h"
#include "queue.h"

typedef struct _Server {
    volatile bool	inited;
    volatile bool	active;
    int			thread_cnt;
    bool		pedantic;
    bool		root_first;
    pthread_t		listen_thread;
    pthread_t		con_thread;
    struct _Queue	con_queue;
    Hook		hooks;
    Hook		hook404;

    //int			port; // TBD remove
    //int			fd; // TBD remove
    Bind		binds;

    // A count of the running threads from the wrapper or the server managed
    // threads.
    atomic_int		running;
} *Server;

extern struct _Server	the_server;

extern void	server_setup();
extern void	server_shutdown(const char *app_name, void (*stop)());
extern void	server_bind(Bind b);

extern int	setup_listen(Err err);
extern int	server_start(Err err, const char *app_name, const char *version);

#endif // __AGOO_SERVER_H__
