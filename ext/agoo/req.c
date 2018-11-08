// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "con.h"
#include "debug.h"
#include "server.h"
#include "req.h"

Req
req_create(size_t mlen) {
    size_t	size = mlen + sizeof(struct _Req) - 7;
    Req		req = (Req)malloc(size);
    
    if (NULL != req) {
	DEBUG_ALLOC(mem_req, req);
	memset(req, 0, size);
	req->env = the_server.env_nil_value;
	req->mlen = mlen;
	req->hook = NULL;
    }
    return req;
}

void
req_destroy(Req req) {
    DEBUG_FREE(mem_req, req);
    if (NULL != req->hook && PUSH_HOOK == req->hook->type) {
	free(req->hook);
    }
    free(req);
}

const char*
req_host(Req r, int *lenp) {
    const char	*host;
    const char	*colon;

    if (NULL == (host = con_header_value(r->header.start, r->header.len, "Host", lenp))) {
	return NULL;
    }
    for (colon = host + *lenp - 1; host < colon; colon--) {
	if (':' == *colon) {
	    break;
	}
    }
    if (host < colon) {
	*lenp = (int)(colon - host);
    }
    return host;
}

int
req_port(Req r) {
    int		len;
    const char	*host;
    const char	*colon;
    
    if (NULL == (host = con_header_value(r->header.start, r->header.len, "Host", &len))) {
	return 0;
    }
    for (colon = host + len - 1; host < colon; colon--) {
	if (':' == *colon) {
	    break;
	}
    }
    if (host == colon) {
	return 0;
    }
    return (int)strtol(colon + 1, NULL, 10);
}

const char*
req_query_value(Req r, const char *key, int klen, int *vlenp) {
    const char	*value;

    if (NULL != (value = strstr(r->query.start, key))) {
	char	*end;

	if (0 >= klen) {
	    klen = (int)strlen(key);
	}
	value += klen + 1;
	if (NULL == (end = index(value, '&'))) {
	    *vlenp = (int)strlen(value);
	} else {
	    *vlenp = (int)(end - value);
	}
    }
    return value;
}

