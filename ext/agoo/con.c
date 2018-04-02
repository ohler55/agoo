// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <ctype.h>
#include <string.h>

#include "con.h"
#include "debug.h"
#include "dtime.h"
#include "hook.h"
#include "http.h"
#include "res.h"
#include "server.h"

#define MAX_SOCK	4096
#define CON_TIMEOUT	5.0

Con
con_create(Err err, Server server, int sock, uint64_t id) {
    Con	c;

    if (MAX_SOCK <= sock) {
	err_set(err, ERR_TOO_MANY, "Too many connections.");
	return NULL;
    }
    if (NULL == (c = (Con)malloc(sizeof(struct _Con)))) {
	err_set(err, ERR_MEMORY, "Failed to allocate memory for a connection.");
    } else {
	DEBUG_ALLOC(mem_con)
	memset(c, 0, sizeof(struct _Con));
	c->sock = sock;
	c->iid = id;
	c->server = server;
	c->kind = CON_HTTP;
    }
    return c;
}

void
con_destroy(Con c) {
    if (0 < c->sock) {
	close(c->sock);
	c->sock = 0;
    }
    if (NULL != c->req) {
	free(c->req);
	DEBUG_FREE(mem_req)
    }
    DEBUG_FREE(mem_con)
    free(c);
}

const char*
con_header_value(const char *header, int hlen, const char *key, int *vlen) {
    // Search for \r then check for \n and then the key followed by a :. Keep
    // trying until the end of the header.
    const char	*h = header;
    const char	*hend = header + hlen;
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

static long
bad_request(Con c, int status, int line) {
    Res		res;
    const char *msg = http_code_message(status);
    
    if (NULL == (res = res_create())) {
	log_cat(&c->server->error_cat, "memory allocation of response failed on connection %llu @ %d.", c->iid, line);
    } else {
	char	buf[256];
	int	cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n", status, msg);
	Text	message = text_create(buf, cnt);
	
	DEBUG_ALLOC(mem_res)
	if (NULL == c->res_tail) {
	    c->res_head = res;
	} else {
	    c->res_tail->next = res;
	}
	c->res_tail = res;
	res->close = true;
	res_set_message(res, message);
    }
    return -1;
}

static bool
should_close(const char *header, int hlen) {
    const char	*v;
    int		vlen = 0;
    
    if (NULL != (v = con_header_value(header, hlen, "Connection", &vlen))) {
	return (5 == vlen && 0 == strncasecmp("Close", v, 5));
    }
    return false;
}

// Returns:
//  0 - when header has not been read
//  message length - when length can be determined
//  -1 on a bad request
//  negative of message length - when message is handled here.
static long
con_header_read(Con c) {
    Server	server = c->server;
    char	*hend = strstr(c->buf, "\r\n\r\n");
    Method	method;
    char	*path;
    char	*pend;
    char	*query = NULL;
    char	*qend;
    char	*b;
    size_t	clen = 0;
    long	mlen;
    Hook	hook = NULL;

    if (NULL == hend) {
	if (sizeof(c->buf) - 1 <= c->bcnt) {
	    return bad_request(c, 431, __LINE__);
	}
	return 0;
    }
    if (server->req_cat.on) {
	*hend = '\0';
	log_cat(&server->req_cat, "%llu: %s", c->iid, c->buf);
	*hend = '\r';
    }
    for (b = c->buf; ' ' != *b; b++) {
	if ('\0' == *b) {
	    return bad_request(c, 400, __LINE__);
	}
    }
    switch (toupper(*c->buf)) {
    case 'G':
	if (3 != b - c->buf || 0 != strncmp("GET", c->buf, 3)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = GET;
	break;
    case 'P': {
	const char	*v;
	int		vlen = 0;
	char		*vend;
	
	if (3 == b - c->buf && 0 == strncmp("PUT", c->buf, 3)) {
	    method = PUT;
	} else if (4 == b - c->buf && 0 == strncmp("POST", c->buf, 4)) {
	    method = POST;
	} else {
	    return bad_request(c, 400, __LINE__);
	}
	if (NULL == (v = con_header_value(c->buf, (int)(hend - c->buf), "Content-Length", &vlen))) {
	    return bad_request(c, 411, __LINE__);
	}
	clen = (size_t)strtoul(v, &vend, 10);
	if (vend != v + vlen) {
	    return bad_request(c, 411, __LINE__);
	}
	break;
    }
    case 'D':
	if (6 != b - c->buf || 0 != strncmp("DELETE", c->buf, 6)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = DELETE;
	break;
    case 'H':
	if (4 != b - c->buf || 0 != strncmp("HEAD", c->buf, 4)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = HEAD;
	break;
    case 'O':
	if (7 != b - c->buf || 0 != strncmp("OPTIONS", c->buf, 7)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = OPTIONS;
	break;
    case 'C':
	if (7 != b - c->buf || 0 != strncmp("CONNECT", c->buf, 7)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = CONNECT;
	break;
    default:
	return bad_request(c, 400, __LINE__);
    }
    for (; ' ' == *b; b++) {
	if ('\0' == *b) {
	    return bad_request(c, 400, __LINE__);
	}
    }
    path = b;
    for (; ' ' != *b; b++) {
	switch (*b) {
	case '?':
	    pend = b;
	    query = b + 1;
	    break;
	case '\0':
	    return bad_request(c, 400, __LINE__);
	default:
	    break;
	}
    }
    if (NULL == query) {
	pend = b;
	query = b;
	qend = b;
    } else {
	qend = b;
    }
    mlen = hend - c->buf + 4 + clen;
    if (NULL == (hook = hook_find(server->hooks, method, path, pend))) {
	if (GET == method) {
	    struct _Err	err = ERR_INIT;
	    Page	p = page_get(&err, &server->pages, server->root, path, (int)(pend - path));
	    Res		res;

	    if (NULL == p) {
		if (NULL != server->hook404) {
		    // There would be too many parameters to pass to a
		    // separate function so just goto the hook processing.
		    hook = server->hook404;
		    goto HOOKED;
		}
		return bad_request(c, 404, __LINE__);
	    }
    	    if (NULL == (res = res_create())) {
		return bad_request(c, 500, __LINE__);
	    }
	    if (NULL == c->res_tail) {
		c->res_head = res;
	    } else {
		c->res_tail->next = res;
	    }
	    c->res_tail = res;

	    b = strstr(c->buf, "\r\n");
	    res->close = should_close(b, (int)(hend - b));

	    text_ref(p->resp);
	    res_set_message(res, p->resp);

	    return -mlen;
	}
    }
HOOKED:
    // Create request and populate.
    if (NULL == (c->req = (Req)malloc(mlen + sizeof(struct _Req) - 8 + 1))) {
	return bad_request(c, 413, __LINE__);
    }
    DEBUG_ALLOC(mem_req)
    if ((long)c->bcnt <= mlen) {
	memcpy(c->req->msg, c->buf, c->bcnt);
	if ((long)c->bcnt < mlen) {
	    memset(c->req->msg + c->bcnt, 0, mlen - c->bcnt);
	}
    } else {
	memcpy(c->req->msg, c->buf, mlen);
    }
    c->req->msg[mlen] = '\0';
    c->req->wrap = Qnil;
    c->req->con = c;
    c->req->method = method;
    c->req->upgrade = UP_NONE;
    c->req->path.start = c->req->msg + (path - c->buf);
    c->req->path.len = (int)(pend - path);
    c->req->query.start = c->req->msg + (query - c->buf);
    c->req->query.len = (int)(qend - query);
    c->req->mlen = mlen;
    c->req->body.start = c->req->msg + (hend - c->buf + 4);
    c->req->body.len = (unsigned int)clen;
    b = strstr(b, "\r\n");
    c->req->header.start = c->req->msg + (b + 2 - c->buf);
    c->req->header.len = (unsigned int)(hend - b - 2);
    c->req->res = NULL;
    if (NULL != hook) {
	c->req->handler = hook->handler;
	c->req->handler_type = hook->type;
    } else {
	c->req->handler = Qnil;
	c->req->handler_type = NO_HOOK;
    }
    return mlen;
}

static bool
con_http_read(Con c) {
    ssize_t	cnt;
    
    if (NULL != c->req) {
	cnt = recv(c->sock, c->req->msg + c->bcnt, c->req->mlen - c->bcnt, 0);
    } else {
	cnt = recv(c->sock, c->buf + c->bcnt, sizeof(c->buf) - c->bcnt - 1, 0);
    }
    c->timeout = dtime() + CON_TIMEOUT;
    if (0 >= cnt) {
	// If nothing read then no need to complain. Just close.
	if (0 < c->bcnt) {
	    if (0 == cnt) {
		log_cat(&c->server->warn_cat, "Nothing to read. Client closed socket on connection %llu.", c->iid);
	    } else {
		log_cat(&c->server->warn_cat, "Failed to read request. %s.", strerror(errno));
	    }
	}
	return true;
    }
    c->bcnt += cnt;
    while (true) {
	if (NULL == c->req) {
	    long	mlen;
	    
	    // Terminate with \0 for debug and strstr() check
	    c->buf[c->bcnt] = '\0';
	    if (0 > (mlen = con_header_read(c))) {
		if (-1 == mlen) {
		    c->bcnt = 0;
		    *c->buf = '\0';
		    return false;
		} else if (-mlen < (long)c->bcnt) {
		    mlen = -mlen;
		    memmove(c->buf, c->buf + mlen, c->bcnt - mlen);
		    c->bcnt -= mlen;
		} else {
		    c->bcnt = 0;
		    *c->buf = '\0';
		    return false;
		}
		continue;
	    }
	}
	if (NULL != c->req) {
	    if (c->req->mlen <= c->bcnt) {
		Res	res;

		if (c->server->debug_cat.on && NULL != c->req && NULL != c->req->body.start) {
		    log_cat(&c->server->debug_cat, "request on %llu: %s", c->iid, c->req->body.start);
		}
		if (NULL == (res = res_create())) {
		    c->req = NULL;
		    log_cat(&c->server->error_cat, "memory allocation of response failed on connection %llu.", c->iid);
		    return bad_request(c, 500, __LINE__);
		} else {
		    if (NULL == c->res_tail) {
			c->res_head = res;
		    } else {
			c->res_tail->next = res;
		    }
		    c->res_tail = res;
		    res->close = should_close(c->req->header.start, c->req->header.len);
		}
		c->req->res = res;
		queue_push(&c->server->eval_queue, (void*)c->req);
		if (c->req->mlen < c->bcnt) {
		    memmove(c->buf, c->buf + c->req->mlen, c->bcnt - c->req->mlen);
		    c->bcnt -= c->req->mlen;
		} else {
		    c->bcnt = 0;
		    *c->buf = '\0';
		    c->req = NULL;
		    break;
		}
		c->req = NULL;
		continue;
	    }
	}
	break;
    }
    return false;
}

static bool
con_ws_read(Con c) {
    // TBD
    return false;
}

static bool
con_sse_read(Con c) {
    // TBD
    return false;
}

// return true to remove/close connection
static bool
con_read(Con c) {
    switch (c->kind) {
    case CON_HTTP:
	return con_http_read(c);
    case CON_WS:
	return con_ws_read(c);
    case CON_SSE:
	return con_sse_read(c);
    default:
	break;
    }
    return true;
}

// return true to remove/close connection
static bool
con_write(Con c) {
    Text	message = res_message(c->res_head);
    ssize_t	cnt;

    c->timeout = dtime() + CON_TIMEOUT;
    if (0 == c->wcnt) {
	if (c->server->resp_cat.on) {
	    char	buf[4096];
	    char	*hend = strstr(message->text, "\r\n\r\n");

	    if (NULL == hend) {
		hend = message->text + message->len;
	    }
	    if ((long)sizeof(buf) <= hend - message->text) {
		hend = message->text + sizeof(buf) - 1;
	    }
	    memcpy(buf, message->text, hend - message->text);
	    buf[hend - message->text] = '\0';
	    log_cat(&c->server->resp_cat, "%llu: %s", c->iid, buf);
	}
	if (c->server->debug_cat.on) {
	    log_cat(&c->server->debug_cat, "response on %llu: %s", c->iid, message->text);
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	if (EAGAIN == errno) {
	    return false;
	}
	log_cat(&c->server->error_cat, "Socket error @ %llu.", c->iid);

	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	Res	res = c->res_head;
	bool	done = res->close;
	
	c->res_head = res->next;
	if (res == c->res_tail) {
	    c->res_tail = NULL;
	}
	c->wcnt = 0;
	res_destroy(res);

	return done;
    }
    return false;
}

void*
con_loop(void *x) {
    Server		server = (Server)x;
    Con			c;
    Con			ca[MAX_SOCK];
    struct pollfd	pa[MAX_SOCK];
    struct pollfd	*pp;
    Con			*end = ca + MAX_SOCK;
    Con			*cp;
    int			ccnt = 0;
    int			i;
    long		mcnt = 0;
    double		now;
    
    atomic_fetch_add(&server->running, 1);
    memset(ca, 0, sizeof(ca));
    memset(pa, 0, sizeof(pa));
    while (server->active) {
	while (NULL != (c = (Con)queue_pop(&server->con_queue, 0.0))) {
	    mcnt++;
	    ca[c->sock] = c;
	    ccnt++;
	}
	pp = pa;
	pp->fd = queue_listen(&server->con_queue);
	pp->events = POLLIN;
	pp->revents = 0;
	pp++;
	for (i = ccnt, cp = ca; 0 < i && cp < end; cp++) {
	    if (NULL == *cp) {
		continue;
	    }
	    c = *cp;
	    c->pp = pp;
	    pp->fd = c->sock;
	    if (NULL != c->res_head && NULL != res_message(c->res_head)) {
		pp->events = POLLIN | POLLOUT;
	    } else {
		pp->events = POLLIN;
	    }
	    pp->revents = 0;
	    i--;
	    pp++;
	}
	if (0 > (i = poll(pa, (nfds_t)(pp - pa), 100))) {
	    if (EAGAIN == errno) {
		continue;
	    }
	    log_cat(&server->error_cat, "Polling error. %s.", strerror(errno));
	    // Either a signal or something bad like out of memory. Might as well exit.
	    break;
	}
	now = dtime();

	if (0 == i) { // nothing to read or write
	    // TBD check for cons to close
	    continue;
	}
	if (0 != (pa->revents & POLLIN)) {
	    queue_release(&server->con_queue);
	    while (NULL != (c = (Con)queue_pop(&server->con_queue, 0.0))) {
		mcnt++;
		ca[c->sock] = c;
		ccnt++;
	    }
	}
	for (i = ccnt, cp = ca; 0 < i && cp < end; cp++) {
	    if (NULL == *cp || 0 == (*cp)->sock || NULL == (*cp)->pp) {
		continue;
	    }
	    c = *cp;
	    i--;
	    pp = c->pp;
	    if (0 != (pp->revents & POLLIN)) {
		if (con_read(c)) {
		    goto CON_RM;
		}
	    }
	    if (0 != (pp->revents & POLLOUT)) {
		if (con_write(c)) {
		    goto CON_RM;
		}
	    }
	    if (0 != (pp->revents & (POLLERR | POLLHUP | POLLNVAL))) {
		if (0 < c->bcnt) {
		    if (0 != (pp->revents & (POLLHUP | POLLNVAL))) {
			log_cat(&server->error_cat, "Socket %llu closed.", c->iid);
		    } else if (!c->closing) {
			log_cat(&server->error_cat, "Socket %llu error. %s", c->iid, strerror(errno));
		    }
		}
		goto CON_RM;
	    }
	    if (0.0 == c->timeout || now < c->timeout) { 
		continue;
	    } else if (c->closing) {
		goto CON_RM;
	    } else {
		c->closing = true;
		c->timeout = now + 0.5;
		//wush_text_set(&c->resp, (char*)close_resp, sizeof(close_resp) - 1, false);
		continue;
	    }
	    continue;
	CON_RM:
	    ca[c->sock] = NULL;
	    ccnt--;
	    log_cat(&server->con_cat, "Connection %llu closed.", c->iid);
	    con_destroy(c);
	}
    }
    atomic_fetch_sub(&server->running, 1);
    
    return NULL;
}

