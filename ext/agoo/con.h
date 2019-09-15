// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_CON_H
#define AGOO_CON_H

#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef HAVE_OPENSSL_SSL_H
#include <openssl/ssl.h>
#endif

#include "err.h"
#include "req.h"
#include "response.h"
#include "server.h"
#include "kinds.h"

#define MAX_HEADER_SIZE	8192

struct _agooUpgraded;
struct _agooReq;
struct _agooRes;
struct _agooBind;
struct _agooQueue;
struct _gqlSub;

typedef struct _agooConLoop {
    struct _agooConLoop	*next;
    struct _agooQueue	pub_queue;
    pthread_t		thread;
    int			id;

    // Cache of responses to be reused.
    struct _agooRes	*res_head;
    struct _agooRes	*res_tail;
    pthread_mutex_t	lock;

} *agooConLoop;

typedef struct _agooCon {
    struct _agooCon		*next;
    int				sock;
    struct _agooBind		*bind;
    struct pollfd		*pp;
    uint64_t			id;
    char			buf[MAX_HEADER_SIZE];
    size_t			bcnt;

    ssize_t			mcnt;  // how much has been read so far
    ssize_t			wcnt;  // how much has been written

    double			timeout;
    bool			closing;
    bool			dead;
    volatile bool		hijacked;
    struct _agooReq		*req;
    struct _agooRes		*res_head;
    struct _agooRes		*res_tail;
    pthread_mutex_t		res_lock;

    struct _agooUpgraded	*up; // only set for push connections
    struct _gqlSub		*gsub; // for graphql subscription
#ifdef HAVE_OPENSSL_SSL_H
    SSL				*ssl;
#endif
    agooConLoop			loop;
} *agooCon;

extern agooCon		agoo_con_create(agooErr err, int sock, uint64_t id, struct _agooBind *b);
extern void		agoo_con_destroy(agooCon c);
extern const char*	agoo_con_header_value(const char *header, int hlen, const char *key, int *vlen);

extern agooConLoop	agoo_conloop_create(agooErr err, int id);
extern void		agoo_conloop_destroy(agooConLoop loop);

extern void		agoo_con_res_append(agooCon c, struct _agooRes *res);

extern bool		agoo_con_http_read(agooCon c);
extern bool		agoo_con_http_write(agooCon c);
extern short		agoo_con_http_events(agooCon c);

#endif // AGOO_CON_H
