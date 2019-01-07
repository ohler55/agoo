// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "con.h"
#include "debug.h"
#include "server.h"
#include "req.h"

agooReq
agoo_req_create(size_t mlen) {
    size_t	size = mlen + sizeof(struct _agooReq) - 7;
    agooReq	req = (agooReq)AGOO_MALLOC(size);
    
    if (NULL != req) {
	memset(req, 0, size);
	req->env = agoo_server.env_nil_value;
	req->mlen = mlen;
	req->hook = NULL;
    }
    return req;
}

void
agoo_req_destroy(agooReq req) {
    if (NULL != req->hook && PUSH_HOOK == req->hook->type) {
	AGOO_FREE(req->hook);
    }
    AGOO_FREE(req);
}

const char*
agoo_req_host(agooReq r, int *lenp) {
    const char	*host;
    const char	*colon;

    if (NULL == (host = agoo_con_header_value(r->header.start, r->header.len, "Host", lenp))) {
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
agoo_req_port(agooReq r) {
    int		len;
    const char	*host;
    const char	*colon;
    
    if (NULL == (host = agoo_con_header_value(r->header.start, r->header.len, "Host", &len))) {
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
agoo_req_query_value(agooReq r, const char *key, int klen, int *vlenp) {
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

static int
hexVal(int c) {
    int	h = -1;
    
    if ('0' <= c && c <= '9') {
	h = c - '0';
    } else if ('a' <= c && c <= 'f') {
	h = c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
	h = c - 'A' + 10;
    }
    return h;
}

int
agoo_req_query_decode(char *s, int len) {
    char	*sn = s;
    char	*so = s;
    char	*end = s + len;
    
    while (so < end) {
	if ('%' == *so) {
	    int	n;
	    int	c = 0;
	    
	    so++;
	    if (0 > (c = hexVal(*so))) {
		*sn++ = '%';
		continue;
	    }
	    so++;
	    if (0 > (n = hexVal(*so))) {
		continue;
	    }
	    c = (c << 4) + n;
	    so++;
	    *sn++ = (char)c;
	} else {
	    *sn++ = *so++;
	}
    }
    *sn = '\0';
    
    return (int)(sn - s);
}

const char*
agoo_req_header_value(agooReq req, const char *key, int *vlen) {
    // Search for \r then check for \n and then the key followed by a :. Keep
    // trying until the end of the header.
    const char	*h = req->header.start;
    const char	*hend = h + req->header.len;
    const char	*value;
    int		klen = (int)strlen(key);
    
    while (h < hend) {
	if (0 == strncmp(key, h, klen) && ':' == h[klen]) {
	    h += klen + 1;
	    for (; ' ' == *h; h++) {
	    }
	    value = h;
	    for (; '\r' != *h && '\0' != *h; h++) {
	    }
	    *vlen = (int)(h - value);
 
	    return value;
	}
	for (; h < hend; h++) {
	    if ('\r' == *h && '\n' == *(h + 1)) {
		h += 2;
		break;
	    }
	}
    }
    return NULL;
}

