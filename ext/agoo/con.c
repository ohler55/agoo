// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <ctype.h>
#include <string.h>

#include "con.h"
#include "debug.h"
#include "dtime.h"
#include "hook.h"
#include "http.h"
#include "pub.h"
#include "res.h"
#include "server.h"
#include "sse.h"
#include "websocket.h"

#define MAX_SOCK	4096
#define CON_TIMEOUT	5.0

Con
con_create(Err err, int sock, uint64_t id) {
    Con	c;

    if (MAX_SOCK <= sock) {
	err_set(err, ERR_TOO_MANY, "Too many connections.");
	return NULL;
    }
    if (NULL == (c = (Con)malloc(sizeof(struct _Con)))) {
	err_set(err, ERR_MEMORY, "Failed to allocate memory for a connection.");
    } else {
	DEBUG_ALLOC(mem_con, c)
	memset(c, 0, sizeof(struct _Con));
	c->sock = sock;
	c->id = id;
	c->kind = CON_HTTP;
	c->timeout = dtime() + CON_TIMEOUT;
    }
    return c;
}

void
con_destroy(Con c) {
    if (CON_WS == c->kind || CON_SSE == c->kind) {
	ws_req_close(c);
    }
    if (0 < c->sock) {
	close(c->sock);
	c->sock = 0;
    }
    if (NULL != c->req) {
	request_destroy(c->req);
    }
    if (NULL != c->slot) {
	cc_remove_con(&the_server.con_cache, c->id);
	c->slot = NULL;
    }
    DEBUG_FREE(mem_con, c)
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
	log_cat(&error_cat, "memory allocation of response failed on connection %llu @ %d.", c->id, line);
    } else {
	char	buf[256];
	int	cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n", status, msg);
	Text	message = text_create(buf, cnt);
	
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
    if (req_cat.on) {
	*hend = '\0';
	log_cat(&req_cat, "%llu: %s", c->id, c->buf);
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
    if (NULL == (hook = hook_find(the_server.hooks, method, path, pend))) {
	if (GET == method) {
	    struct _Err	err = ERR_INIT;
	    Page	p = page_get(&err, &the_server.pages, the_server.root, path, (int)(pend - path));
	    Res		res;

	    if (NULL == p) {
		if (NULL != the_server.hook404) {
		    // There would be too many parameters to pass to a
		    // separate function so just goto the hook processing.
		    hook = the_server.hook404;
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
	    res_set_message(res, p->resp);

	    return -mlen;
	}
    }
HOOKED:
    // Create request and populate.
    if (NULL == (c->req = request_create(mlen))) {
	return bad_request(c, 413, __LINE__);
    }
    if ((long)c->bcnt <= mlen) {
	memcpy(c->req->msg, c->buf, c->bcnt);
	if ((long)c->bcnt < mlen) {
	    memset(c->req->msg + c->bcnt, 0, mlen - c->bcnt);
	}
    } else {
	memcpy(c->req->msg, c->buf, mlen);
    }
    c->req->msg[mlen] = '\0';
    c->req->method = method;
    c->req->upgrade = UP_NONE;
    c->req->cid = c->id;
    c->req->path.start = c->req->msg + (path - c->buf);
    c->req->path.len = (int)(pend - path);
    c->req->query.start = c->req->msg + (query - c->buf);
    c->req->query.len = (int)(qend - query);
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

static void
check_upgrade(Con c) {
    const char	*v;
    int		vlen = 0;
    
    if (NULL == c->req) {
	return;
    }
    if (NULL != (v = con_header_value(c->req->header.start, c->req->header.len, "Connection", &vlen))) {
	if (NULL != strstr(v, "Upgrade")) {
	    if (NULL != (v = con_header_value(c->req->header.start, c->req->header.len, "Upgrade", &vlen))) {
		if (0 == strncasecmp("WebSocket", v, vlen)) {
		    c->res_tail->close = false;
		    c->res_tail->con_kind = CON_WS;
		    return;
		}
	    }
	}
    }
    if (NULL != (v = con_header_value(c->req->header.start, c->req->header.len, "Accept", &vlen))) {
	if (0 == strncasecmp("text/event-stream", v, vlen)) {
	    c->res_tail->close = false;
	    c->res_tail->con_kind = CON_SSE;
	    return;
	}
    }
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
		log_cat(&warn_cat, "Nothing to read. Client closed socket on connection %llu.", c->id);
	    } else {
		log_cat(&warn_cat, "Failed to read request. %s.", strerror(errno));
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
		Req	req;
		Res	res;
		long	mlen;

		if (debug_cat.on && NULL != c->req && NULL != c->req->body.start) {
		    log_cat(&debug_cat, "request on %llu: %s", c->id, c->req->body.start);
		}
		if (NULL == (res = res_create())) {
		    c->req = NULL;
		    log_cat(&error_cat, "memory allocation of response failed on connection %llu.", c->id);
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
		mlen = c->req->mlen;
		check_upgrade(c);
		req = c->req;
		c->req = NULL;
		queue_push(&the_server.eval_queue, (void*)req);
		if (mlen < (long)c->bcnt) {
		    memmove(c->buf, c->buf + mlen, c->bcnt - mlen);
		    c->bcnt -= mlen;
		} else {
		    c->bcnt = 0;
		    *c->buf = '\0';
		    break;
		}
		continue;
	    }
	}
	break;
    }
    return false;
}

static bool
con_ws_read(Con c) {
    ssize_t	cnt;
    uint8_t	*b;
    uint8_t	op;
    long	mlen;	

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
		log_cat(&warn_cat, "Nothing to read. Client closed socket on connection %llu.", c->id);
	    } else {
		log_cat(&warn_cat, "Failed to read WebSocket message. %s.", strerror(errno));
	    }
	}
	return true;
    }
    c->bcnt += cnt;
    while (true) {
	if (NULL == c->req) {
	    if (c->bcnt < 2) {
		return false; // Try again.
	    }
	    b = (uint8_t*)c->buf;
	    if (0 >= (mlen = ws_calc_len(c, b, c->bcnt))) {
		return (mlen < 0);
	    }
	    op = 0x0F & *b;
	    switch (op) {
	    case WS_OP_TEXT:
	    case WS_OP_BIN:
		if (ws_create_req(c, mlen)) {
		    return true;
		}
		break;
	    case WS_OP_CLOSE:
		return true;
	    case WS_OP_PING:
		ws_pong(c);
		break;
	    case WS_OP_PONG:
		// ignore
		break;
	    case WS_OP_CONT:
	    default:
		log_cat(&error_cat, "WebSocket op 0x%02x not supported on %llu.", op, c->id);
		return true;
	    }
	}
	if (NULL != c->req) {
	    mlen = c->req->mlen;
	    c->req->mlen = ws_decode(c->req->msg, c->req->mlen);
	    if (mlen <= (long)c->bcnt) {
		if (debug_cat.on) {
		    if (ON_MSG == c->req->method) {
			log_cat(&debug_cat, "WebSocket message on %llu: %s", c->id, c->req->msg);
		    } else {
			log_cat(&debug_cat, "WebSocket binary message on %llu", c->id);
		    }
		}
	    }
	    queue_push(&the_server.eval_queue, (void*)c->req);
	    if (mlen < (long)c->bcnt) {
		memmove(c->buf, c->buf + mlen, c->bcnt - mlen);
		c->bcnt -= mlen;
	    } else {
		c->bcnt = 0;
		*c->buf = '\0';
		c->req = NULL;
		break;
	    }
	    c->req = NULL;
	    continue;
	}
	break;
    }
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
    default:
	break;
    }
    return true;
}

// return true to remove/close connection
static bool
con_http_write(Con c) {
    Text	message = res_message(c->res_head);
    ssize_t	cnt;

    c->timeout = dtime() + CON_TIMEOUT;
    if (0 == c->wcnt) {
	if (resp_cat.on) {
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
	    log_cat(&resp_cat, "%llu: %s", c->id, buf);
	}
	if (debug_cat.on) {
	    log_cat(&debug_cat, "response on %llu: %s", c->id, message->text);
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	if (EAGAIN == errno) {
	    return false;
	}
	log_cat(&error_cat, "Socket error @ %llu.", c->id);

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

static const char	ping_msg[] = "\x89\x00";
static const char	pong_msg[] = "\x8a\x00";

static bool
con_ws_write(Con c) {
    Res		res = c->res_head;
    Text	message = res_message(res);
    ssize_t	cnt;

    if (NULL == message) {
	if (res->ping) {
	    if (0 > (cnt = send(c->sock, ping_msg, sizeof(ping_msg) - 1, 0))) {
		if (EAGAIN == errno) {
		    return false;
		}
		log_cat(&error_cat, "Socket error @ %llu.", c->id);
		ws_req_close(c);
		res_destroy(res);
	
		return true;
	    }
	} else if (res->pong) {
	    if (0 > (cnt = send(c->sock, pong_msg, sizeof(pong_msg) - 1, 0))) {
		if (EAGAIN == errno) {
		    return false;
		}
		log_cat(&error_cat, "Socket error @ %llu.", c->id);
		ws_req_close(c);
		res_destroy(res);
	
		return true;
	    }
	} else {
	    ws_req_close(c);
	    res_destroy(res);
	    return true;
	}
	c->res_head = res->next;
	if (res == c->res_tail) {
	    c->res_tail = NULL;
	}
	return false;
    }
    c->timeout = dtime() + CON_TIMEOUT;
    if (0 == c->wcnt) {
	Text	t;
	
	if (push_cat.on) {
	    if (message->bin) {
		log_cat(&push_cat, "%llu binary", c->id);
	    } else {
		log_cat(&push_cat, "%llu: %s", c->id, message->text);
	    }
	}
	t = ws_expand(message);
	if (t != message) {
	    res_set_message(res, t);
	    message = t;
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	if (EAGAIN == errno) {
	    return false;
	}
	log_cat(&error_cat, "Socket error @ %llu.", c->id);
	ws_req_close(c);
	
	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	Res	res = c->res_head;
	bool	done = res->close;
	int	pending;
	
	c->res_head = res->next;
	if (res == c->res_tail) {
	    c->res_tail = NULL;
	}
	c->wcnt = 0;
	res_destroy(res);
	if (1 == (pending = atomic_fetch_sub(&c->slot->pending, 1))) {
	    if (NULL != c->slot && Qnil != c->slot->handler && c->slot->on_empty) {
		Req	req = request_create(0);
	    
		req->cid = c->id;
		req->method = ON_EMPTY;
		req->handler_type = PUSH_HOOK;
		req->handler = c->slot->handler;
		queue_push(&the_server.eval_queue, (void*)req);
	    }
	}
	return done;
    }
    return false;
}

static bool
con_sse_write(Con c) {
    Res		res = c->res_head;
    Text	message = res_message(res);
    ssize_t	cnt;

    if (NULL == message) {
	ws_req_close(c);
	res_destroy(res);
	return true;
    }
    c->timeout = dtime() + CON_TIMEOUT *2;
    if (0 == c->wcnt) {
	Text	t;
	
	if (push_cat.on) {
	    log_cat(&push_cat, "%llu: %s", c->id, message->text);
	}
	t = sse_expand(message);
	if (t != message) {
	    res_set_message(res, t);
	    message = t;
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	if (EAGAIN == errno) {
	    return false;
	}
	log_cat(&error_cat, "Socket error @ %llu.", c->id);
	ws_req_close(c);
	
	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	Res	res = c->res_head;
	bool	done = res->close;
	int	pending;
	
	c->res_head = res->next;
	if (res == c->res_tail) {
	    c->res_tail = NULL;
	}
	c->wcnt = 0;
	res_destroy(res);
	if (1 == (pending = atomic_fetch_sub(&c->slot->pending, 1))) {
	    if (NULL != c->slot && Qnil != c->slot->handler && c->slot->on_empty) {
		Req	req = request_create(0);
	    
		req->cid = c->id;
		req->method = ON_EMPTY;
		req->handler_type = PUSH_HOOK;
		req->handler = c->slot->handler;
		queue_push(&the_server.eval_queue, (void*)req);
	    }
	}
	return done;
    }
    return false;
}

static bool
con_write(Con c) {
    bool	remove = true;
    ConKind	kind = c->res_head->con_kind;

    switch (c->kind) {
    case CON_HTTP:
	remove = con_http_write(c);
	break;
    case CON_WS:
	remove = con_ws_write(c);
	break;
    case CON_SSE:
	remove = con_sse_write(c);
	break;
    default:
	break;
    }
    if (kind != c->kind) {
	c->kind = kind;
	if (CON_HTTP != kind && !remove) {
	    c->slot = cc_set_con(&the_server.con_cache, c);
	}
    }
    return remove;
}

static void
process_pub_con(Pub pub) {
    CSlot	slot;

    switch (pub->kind) {
    case PUB_CLOSE:
	if (NULL == (slot = cc_get_slot(&the_server.con_cache, pub->cid)) || NULL == slot->con) {
	    log_cat(&warn_cat, "Socket %llu already closed.", pub->cid);
	    pub_destroy(pub);	
	    return;
	} else {
	    Res	res = res_create();;

	    if (NULL != res) {
		if (NULL == slot->con->res_tail) {
		    slot->con->res_head = res;
		} else {
		    slot->con->res_tail->next = res;
		}
		slot->con->res_tail = res;
		res->con_kind = slot->con->kind;
		res->close = true;
	    }
	}
	break;
    case PUB_WRITE: {
	if (NULL == (slot = cc_get_slot(&the_server.con_cache, pub->cid)) || NULL == slot->con) {
	    log_cat(&warn_cat, "Socket %llu already closed. WebSocket write failed.", pub->cid);
	    pub_destroy(pub);	
	    return;
	} else {
	    Res	res = res_create();

	    if (NULL != res) {
		if (NULL == slot->con->res_tail) {
		    slot->con->res_head = res;
		} else {
		    slot->con->res_tail->next = res;
		}
		slot->con->res_tail = res;
		res->con_kind = slot->con->kind;
		res_set_message(res, pub->msg);
	    }
	}
	break;
    case PUB_SUB:
	// TBD handle a subscribe when implementing pub/sub 
	break;
    case PUB_UN:
	// TBD handle a subscribe when implementing pub/sub 
	break;
    case PUB_MSG:
	// TBD handle a subscribe when implementing pub/sub 
	break;
    }
    default:
	break;
    }
    pub_destroy(pub);	
}

static struct pollfd*
poll_setup(Con *ca, int ccnt, struct pollfd *pp) {
    Con		*cp;
    Con		*end = ca + MAX_SOCK;
    Con		c;
    int		i;

    // The first two pollfd are for the con_queue and the pub_queue in that
    // order.
    pp->fd = queue_listen(&the_server.con_queue);
    pp->events = POLLIN;
    pp->revents = 0;
    pp++;
    pp->fd = queue_listen(&the_server.pub_queue);
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
	pp->events = 0;
	switch (c->kind) {
	case CON_HTTP:
	    if (NULL != c->res_head && NULL != res_message(c->res_head)) {
		pp->events = POLLIN | POLLOUT;
	    } else {
		pp->events = POLLIN;
	    }
	    break;
	case CON_WS:
	    if (NULL != c->res_head && (c->res_head->close || c->res_head->ping || NULL != res_message(c->res_head))) {
		pp->events = POLLIN | POLLOUT;
	    } else {
		pp->events = POLLIN;
	    }
	    break;
	case CON_SSE:
	    if (NULL != c->res_head && NULL != res_message(c->res_head)) {
		pp->events = POLLOUT;
	    }
	    break;
	default:
	    continue; // should never get here
	    break;
	}
	pp->revents = 0;
	i--;
	pp++;
    }
    return pp;
}

void*
con_loop(void *x) {
    Con			c;
    Con			ca[MAX_SOCK + 2];
    struct pollfd	pa[MAX_SOCK + 2];
    struct pollfd	*pp;
    Con			*end = ca + MAX_SOCK;
    Con			*cp;
    int			ccnt = 0;
    int			i;
    long		mcnt = 0;
    double		now;
    Pub			pub;
    
    atomic_fetch_add(&the_server.running, 1);
    memset(ca, 0, sizeof(ca));
    memset(pa, 0, sizeof(pa));
    while (the_server.active) {
	while (NULL != (c = (Con)queue_pop(&the_server.con_queue, 0.0))) {
	    mcnt++;
	    ca[c->sock] = c;
	    ccnt++;
	}
	while (NULL != (pub = (Pub)queue_pop(&the_server.pub_queue, 0.0))) {
	    process_pub_con(pub);
	}
	pp = poll_setup(ca, ccnt, pa);
	if (0 > (i = poll(pa, (nfds_t)(pp - pa), 100))) {
	    if (EAGAIN == errno) {
		continue;
	    }
	    log_cat(&error_cat, "Polling error. %s.", strerror(errno));
	    // Either a signal or something bad like out of memory. Might as well exit.
	    break;
	}
	now = dtime();

	if (0 < i) {
	    // Check con_queue if an event is waiting.
	    if (0 != (pa->revents & POLLIN)) {
		queue_release(&the_server.con_queue);
		while (NULL != (c = (Con)queue_pop(&the_server.con_queue, 0.0))) {
		    mcnt++;
		    ca[c->sock] = c;
		    ccnt++;
		}
	    }
	    // Check pub_queue if an event is waiting.
	    if (0 != (pa[1].revents & POLLIN)) {
		queue_release(&the_server.pub_queue);
		while (NULL != (pub = (Pub)queue_pop(&the_server.pub_queue, 0.0))) {
		    process_pub_con(pub);
		}
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
			log_cat(&error_cat, "Socket %llu closed.", c->id);
		    } else if (!c->closing) {
			log_cat(&error_cat, "Socket %llu error. %s", c->id, strerror(errno));
		    }
		}
		goto CON_RM;
	    }
	    if (0.0 == c->timeout || now < c->timeout) { 
		continue;
	    } else if (c->closing) {
		goto CON_RM;
	    } else if (CON_WS == c->kind || CON_SSE == c->kind) {
		c->timeout = dtime() + CON_TIMEOUT;
		ws_ping(c);
		continue;
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
	    log_cat(&con_cat, "Connection %llu closed.", c->id);
	    con_destroy(c);
	}
    }
    for (i = ccnt, cp = ca; 0 < i && cp < end; cp++) {
	if (NULL != *cp) {
	    con_destroy(*cp);
	}
    }
    atomic_fetch_sub(&the_server.running, 1);
    
    return NULL;
}

