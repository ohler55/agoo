// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_REQUEST_H__
#define __AGOO_REQUEST_H__

#include <stdint.h>

#include <ruby.h>

#include "hook.h"
#include "res.h"
#include "types.h"

struct _Server;
struct _Upgraded;

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
    VALUE		handler;
    HookType		handler_type;
    Res			res;
    size_t		mlen;   // allocated msg length
    char		msg[8]; // expanded to be full message
} *Req;

extern Req	request_create(size_t mlen);
extern void	request_init(VALUE mod);
extern VALUE	request_wrap(Req req);
extern VALUE	request_env(Req req);
extern void	request_destroy(Req req);

#endif // __AGOO_REQUEST_H__
