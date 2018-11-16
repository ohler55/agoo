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

struct _agooUpgraded;
struct _agooReq;
struct _agooRes;
struct _agooBind;
struct _agooQueue;

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

    struct _agooUpgraded	*up; // only set for push connections
} *agooCon;

typedef struct _agooConLoop {
    struct _agooConLoop	*next;
    struct _agooQueue	pub_queue;
    pthread_t		thread;
    int			id;
} *agooConLoop;
    
extern agooCon		con_create(agooErr err, int sock, uint64_t id, struct _agooBind *b);
extern void		con_destroy(agooCon c);
extern const char*	con_header_value(const char *header, int hlen, const char *key, int *vlen);

extern agooConLoop	conloop_create(agooErr err, int id);

extern bool		con_http_read(agooCon c);
extern bool		con_http_write(agooCon c);
extern short		con_http_events(agooCon c);

#endif // AGOO_CON_H
