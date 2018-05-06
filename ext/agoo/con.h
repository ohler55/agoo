// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_CON_H__
#define __AGOO_CON_H__

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "request.h"
#include "response.h"
#include "server.h"
#include "types.h"

#define MAX_HEADER_SIZE	8192

struct _Upgraded;

typedef struct _Con {
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
    Req			req;
    Res			res_head;
    Res			res_tail;

    struct _Upgraded	*up; // only set for push connections
} *Con;

extern Con		con_create(Err err, int sock, uint64_t id);
extern void		con_destroy(Con c);
extern const char*	con_header_value(const char *header, int hlen, const char *key, int *vlen);

extern void*		con_loop(void *ctx);

#endif /* __AGOO_CON_H__ */
