// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SERVER_H
#define AGOO_SERVER_H

#include <pthread.h>
#include <stdbool.h>

#ifdef HAVE_OPENSSL_SSL_H
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "atomic.h"
#include "bind.h"
#include "err.h"
#include "gqleval.h"
#include "hook.h"
#include "queue.h"

struct _agooCon;
struct _agooConLoop;
struct _agooPub;
struct _agooReq;
struct _agooUpgraded;
struct _gqlSub;
struct _gqlValue;

typedef struct _agooServer {
    volatile bool		inited;
    volatile bool		active;
    int				thread_cnt;
    bool			pedantic;
    bool			root_first;
    bool			rack_early_hints;
    bool			tls;
    pthread_t			listen_thread;
    struct _agooQueue		con_queue;
    agooHook			hooks;
    agooHook			hook404;
    agooBind			binds;

    struct _agooQueue		eval_queue;

    struct _agooConLoop		*con_loops;
    int				loop_max;
    int				loop_cnt;
    atomic_int			con_cnt;

    struct _agooUpgraded	*up_list;
    struct _gqlSub		*gsub_list;
    pthread_mutex_t		up_lock;
    int				max_push_pending;
    void			*env_nil_value;
    void			*ctx_nil_value;

#ifdef HAVE_OPENSSL_SSL_H
    SSL_CTX			*ssl_ctx;
#endif
    // A count of the running threads from the wrapper or the server managed
    // threads.
    atomic_int			running;
} *agooServer;

extern int	agoo_server_setup(agooErr err);
extern void	agoo_server_shutdown(const char *app_name, void (*stop)());
extern void	agoo_server_bind(agooBind b);
extern int	agoo_server_ssl_init(agooErr err, const char *cert_pem, const char *key_pem);

extern int	setup_listen(agooErr err);
extern int	agoo_server_start(agooErr err, const char *app_name, const char *version);

extern void	agoo_server_add_upgraded(struct _agooUpgraded *up);
extern int	agoo_server_add_func_hook(agooErr	err,
					  agooMethod	method,
					  const char	*pattern,
					  void		(*func)(struct _agooReq *req),
					  agooQueue	queue,
					  bool		quick);

extern void	agoo_server_publish(struct _agooPub *pub);

extern void	agoo_server_add_gsub(struct _gqlSub *sub);
extern void	agoo_server_del_gsub(struct _gqlSub *sub);
extern int	agoo_server_gpublish(agooErr err, const char *subject, gqlRef event);

extern struct _agooServer	agoo_server;

extern double	agoo_io_loop_ratio;

#endif // AGOO_SERVER_H
