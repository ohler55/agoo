// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_REQ_H__
#define __AGOO_REQ_H__

#include <stdint.h>

#include "hook.h"
#include "types.h"

struct _Server;
struct _Upgraded;
struct _Res;

typedef enum {
    UP_NONE	= '\0',
    UP_WS	= 'W',
    UP_SSE	= 'S',
} Upgrade;

typedef struct _Str {
    char		*start;
    unsigned int	len;
} *Str;

typedef struct _Req {
    Method		method;
    Upgrade		upgrade;
    struct _Upgraded	*up;
    struct _Str		path;
    struct _Str		query;
    struct _Str		header;
    struct _Str		body;
    Hook		hook;
    void		*env;
    struct _Res		*res;
    size_t		mlen;   // allocated msg length
    char		msg[8]; // expanded to be full message
} *Req;

extern Req		req_create(size_t mlen);
extern void		req_destroy(Req req);
extern const char*	req_host(Req r, int *lenp);
extern int		req_port(Req r);

#endif // __AGOO_REQ_H__
