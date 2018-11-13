// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SERVER_H
#define AGOO_SERVER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "bind.h"
#include "err.h"
#include "hook.h"
#include "queue.h"

struct _ConLoop;
struct _Pub;
struct _Req;
struct _Upgraded;

typedef struct _Server {
    volatile bool	inited;
    volatile bool	active;
    int			thread_cnt;
    bool		pedantic;
    bool		root_first;
    pthread_t		listen_thread;
    struct _Queue	con_queue;
    Hook		hooks;
    Hook		hook404;
    Bind		binds;

    struct _Queue	eval_queue;

    struct _ConLoop	*con_loops;
    int			loop_cnt;
    atomic_int		con_cnt;
    
    struct _Upgraded	*up_list;
    pthread_mutex_t	up_lock;
    int			max_push_pending;
    void		*env_nil_value;
    void		*ctx_nil_value;
    
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

extern void	server_add_upgraded(struct _Upgraded *up);
extern int	server_add_func_hook(Err	err,
				     Method	method,
				     const char	*pattern,
				     void	(*func)(struct _Req *req),
				     Queue	queue,
				     bool	quick);

extern void	server_publish(struct _Pub *pub);

#endif // AGOO_SERVER_H
