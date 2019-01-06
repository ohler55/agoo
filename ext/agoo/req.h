// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_REQ_H
#define AGOO_REQ_H

#include <stdint.h>

#include "hook.h"
#include "kinds.h"

struct _agooUpgraded;
struct _agooRes;

typedef enum {
    AGOO_UP_NONE	= '\0',
    AGOO_UP_WS		= 'W',
    AGOO_UP_SSE		= 'S',
} agooUpgrade;

typedef struct _agooStr {
    char		*start;
    unsigned int	len;
} *agooStr;

typedef struct _agooReq {
    agooMethod			method;
    struct _agooRes		*res;

    agooUpgrade			upgrade;
    struct _agooUpgraded	*up;
    struct _agooStr		path;
    struct _agooStr		query;
    struct _agooStr		header;
    struct _agooStr		body;
    void			*env;
    agooHook			hook;
    size_t			mlen;   // allocated msg length
    char			msg[8]; // expanded to be full message
} *agooReq;

extern agooReq		agoo_req_create(size_t mlen);
extern void		agoo_req_destroy(agooReq req);
extern const char*	agoo_req_host(agooReq r, int *lenp);
extern int		agoo_req_port(agooReq r);
extern const char*	agoo_req_query_value(agooReq r, const char *key, int klen, int *vlenp);
extern int		agoo_req_query_decode(char *s, int len);
const char*		agoo_req_header_value(agooReq req, const char *key, int *vlen);

#endif // AGOO_REQ_H
