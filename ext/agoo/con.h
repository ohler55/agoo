// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_CON_H__
#define __AGOO_CON_H__

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "req.h"
#include "response.h"
#include "server.h"
#include "types.h"

#define MAX_HEADER_SIZE	8192

typedef enum {
    CON_ANY	= '\0',
    CON_HTTP	= 'H',
    CON_WS	= 'W',
    CON_SSE	= 'S',
} ConKind;

struct _Upgraded;
struct _Req;
struct _Res;

typedef struct _Con {
    struct _Con		*next;
    int			sock;
    ConKind		kind;
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

extern Con		con_create(Err err, int sock, uint64_t id);
extern void		con_destroy(Con c);
extern const char*	con_header_value(const char *header, int hlen, const char *key, int *vlen);

extern void*		con_loop(void *ctx);

#endif /* __AGOO_CON_H__ */
