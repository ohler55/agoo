// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_CON_H
#define AGOO_CON_H

#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "req.h"
#include "response.h"
#include "server.h"
#include "kinds.h"

#define MAX_HEADER_SIZE	8192

struct _Upgraded;
struct _Req;
struct _Res;
struct _Bind;
struct _Queue;

typedef struct _Con {
    struct _Con		*next;
    int			sock;
    struct _Bind	*bind;
    struct pollfd	*pp;
    uint64_t		id;
    char		buf[MAX_HEADER_SIZE];
    size_t		bcnt;

    ssize_t		mcnt;  // how much has been read so far
    ssize_t		wcnt;  // how much has been written

    double		timeout;
    bool		closing;
    bool		dead;
    volatile bool	hijacked;
    struct _Req		*req;
    struct _Res		*res_head;
    struct _Res		*res_tail;

    struct _Upgraded	*up; // only set for push connections
} *Con;

typedef struct _ConLoop {
    struct _ConLoop	*next;
    struct _Queue	pub_queue;
    pthread_t		thread;
    int			id;
} *ConLoop;
    
extern Con		con_create(Err err, int sock, uint64_t id, struct _Bind *b);
extern void		con_destroy(Con c);
extern const char*	con_header_value(const char *header, int hlen, const char *key, int *vlen);

extern struct _ConLoop*	conloop_create(Err err, int id);

extern bool		con_http_read(Con c);
extern bool		con_http_write(Con c);
extern short		con_http_events(Con c);

#endif /* AGOO_CON_H */
