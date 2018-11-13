// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_REQ_H
#define AGOO_REQ_H

#include <stdint.h>

#include "hook.h"
#include "kinds.h"

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
    struct _Res		*res;

    Upgrade		upgrade;
    struct _Upgraded	*up;
    struct _Str		path;
    struct _Str		query;
    struct _Str		header;
    struct _Str		body;
    void		*env;
    Hook		hook;
    size_t		mlen;   // allocated msg length
    char		msg[8]; // expanded to be full message
} *Req;

extern Req		req_create(size_t mlen);
extern void		req_destroy(Req req);
extern const char*	req_host(Req r, int *lenp);
extern int		req_port(Req r);
extern const char*	req_query_value(Req r, const char *key, int klen, int *vlenp);

#endif // AGOO_REQ_H
