// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bind.h"
#include "con.h"
#include "debug.h"
#include "dtime.h"
#include "hook.h"
#include "http.h"
#include "log.h"
#include "page.h"
#include "pub.h"
#include "res.h"
#include "seg.h"
#include "server.h"
#include "sse.h"
#include "subject.h"
#include "upgraded.h"
#include "websocket.h"

#define CON_TIMEOUT		5.0
#define INITIAL_POLL_SIZE	1024

static bool	con_ws_read(agooCon c);
static bool	con_ws_write(agooCon c);
static short	con_ws_events(agooCon c);
static bool	con_sse_write(agooCon c);
static short	con_sse_events(agooCon c);

static struct _agooBind	ws_bind = {
    .kind = CON_WS,
    .read = con_ws_read,
    .write = con_ws_write,
    .events = con_ws_events,
};

static struct _agooBind	sse_bind = {
    .kind = CON_SSE,
    .read = NULL,
    .write = con_sse_write,
    .events = con_sse_events,
};

agooCon
con_create(agooErr err, int sock, uint64_t id, agooBind b) {
    agooCon	c;

    if (NULL == (c = (agooCon)malloc(sizeof(struct _agooCon)))) {
	err_set(err, ERR_MEMORY, "Failed to allocate memory for a connection.");
    } else {
	DEBUG_ALLOC(mem_con, c)
	memset(c, 0, sizeof(struct _agooCon));
	c->sock = sock;
	c->id = id;
	c->timeout = dtime() + CON_TIMEOUT;
	c->bind = b;
    }
    return c;
}

void
con_destroy(agooCon c) {
    atomic_fetch_sub(&the_server.con_cnt, 1);

    if (CON_WS == c->bind->kind || CON_SSE == c->bind->kind) {
	ws_req_close(c);
    }
    if (0 < c->sock) {
	close(c->sock);
	c->sock = 0;
    }
    if (NULL != c->req) {
	req_destroy(c->req);
    }
    if (NULL != c->up) {
	upgraded_release_con(c->up);
	c->up = NULL;
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
bad_request(agooCon c, int status, int line) {
    agooRes	res;
    const char *msg = http_code_message(status);
    
    if (NULL == (res = res_create(c))) {
	log_cat(&error_cat, "memory allocation of response failed on connection %llu @ %d.", (unsigned long long)c->id, line);
    } else {
	char	buf[256];
	int	cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n", status, msg);
	agooText	message = text_create(buf, cnt);
	
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

static bool
page_response(agooCon c, agooPage p, char *hend) {
    agooRes 	res;
    char	*b;
    
    if (NULL == (res = res_create(c))) {
	return true;
    }
    if (NULL == c->res_tail) {
	c->res_head = res;
    } else {
	c->res_tail->next = res;
    }
    c->res_tail = res;

    b = strstr(c->buf, "\r\n");
    res->close = should_close(b, (int)(hend - b));
    if (res->close) {
	c->closing = true;
    }
    res_set_message(res, p->resp);

    return false;
}

// rserver
static void
push_error(agooUpgraded up, const char *msg, int mlen) {
    if (NULL != up && the_server.ctx_nil_value != up->ctx && up->on_error) {
	agooReq	req = req_create(mlen);

	if (NULL == req) {
	    return;
	}
	memcpy(req->msg, msg, mlen);
	req->msg[mlen] = '\0';
	req->up = up;
	req->method = AGOO_ON_ERROR;
	req->hook = hook_create(AGOO_NONE, NULL, up->ctx, PUSH_HOOK, &the_server.eval_queue);
	upgraded_ref(up);
	queue_push(&the_server.eval_queue, (void*)req);
    }
}

// Returns:
//  0 - when header has not been read
//  message length - when length can be determined
//  -1 on a bad request
//  negative of message length - when message is handled here.
static long
con_header_read(agooCon c) {
    char		*hend = strstr(c->buf, "\r\n\r\n");
    agooMethod		method;
    struct _agooSeg	path;
    char		*query = NULL;
    char		*qend;
    char		*b;
    size_t		clen = 0;
    long		mlen;
    agooHook		hook = NULL;
    agooPage		p;
    struct _agooErr	err = ERR_INIT;
    
    if (NULL == hend) {
	if (sizeof(c->buf) - 1 <= c->bcnt) {
	    return bad_request(c, 431, __LINE__);
	}
	return 0;
    }
    if (req_cat.on) {
	*hend = '\0';
	log_cat(&req_cat, "%llu: %s", (unsigned long long)c->id, c->buf);
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
	method = AGOO_GET;
	break;
    case 'P': {
	const char	*v;
	int		vlen = 0;
	char		*vend;
	
	if (3 == b - c->buf && 0 == strncmp("PUT", c->buf, 3)) {
	    method = AGOO_PUT;
	} else if (4 == b - c->buf && 0 == strncmp("POST", c->buf, 4)) {
	    method = AGOO_POST;
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
	method = AGOO_DELETE;
	break;
    case 'H':
	if (4 != b - c->buf || 0 != strncmp("HEAD", c->buf, 4)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = AGOO_HEAD;
	break;
    case 'O':
	if (7 != b - c->buf || 0 != strncmp("OPTIONS", c->buf, 7)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = AGOO_OPTIONS;
	break;
    case 'C':
	if (7 != b - c->buf || 0 != strncmp("CONNECT", c->buf, 7)) {
	    return bad_request(c, 400, __LINE__);
	}
	method = AGOO_CONNECT;
	break;
    default:
	return bad_request(c, 400, __LINE__);
    }
    for (; ' ' == *b; b++) {
	if ('\0' == *b) {
	    return bad_request(c, 400, __LINE__);
	}
    }
    path.start = b;
    for (; ' ' != *b; b++) {
	switch (*b) {
	case '?':
	    path.end = b;
	    query = b + 1;
	    break;
	case '\0':
	    return bad_request(c, 400, __LINE__);
	default:
	    break;
	}
    }
    if (NULL == query) {
	path.end = b;
	query = b;
	qend = b;
    } else {
	qend = b;
    }
    mlen = hend - c->buf + 4 + clen;
    if (AGOO_GET == method &&
	NULL != (p = group_get(&err, path.start, (int)(path.end - path.start)))) {
	if (page_response(c, p, hend)) {
	    return bad_request(c, 500, __LINE__);
	}
	return -mlen;
    }
    if (AGOO_GET == method && the_server.root_first &&
	NULL != (p = page_get(&err, path.start, (int)(path.end - path.start)))) {
	if (page_response(c, p, hend)) {
	    return bad_request(c, 500, __LINE__);
	}
	return -mlen;
    }
    if (NULL == (hook = hook_find(the_server.hooks, method, &path))) {
	if (AGOO_GET == method) {
	    if (the_server.root_first) { // already checked
		if (NULL != the_server.hook404) {
		    // There would be too many parameters to pass to a
		    // separate function so just goto the hook processing.
		    hook = the_server.hook404;
		    goto HOOKED;
		}
		return bad_request(c, 404, __LINE__);
	    }
	    if (NULL == (p = page_get(&err, path.start, (int)(path.end - path.start)))) {
		if (NULL != the_server.hook404) {
		    // There would be too many parameters to pass to a
		    // separate function so just goto the hook processing.
		    hook = the_server.hook404;
		    goto HOOKED;
		}
		return bad_request(c, 404, __LINE__);
	    }
	    if (page_response(c, p, hend)) {
		return bad_request(c, 500, __LINE__);
	    }
	    return -mlen;
	}
 	return bad_request(c, 404, __LINE__);
   }
HOOKED:
    // Create request and populate.
    if (NULL == (c->req = req_create(mlen))) {
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
    c->req->upgrade = AGOO_UP_NONE;
    c->req->up = NULL;
    c->req->path.start = c->req->msg + (path.start - c->buf);
    c->req->path.len = (int)(path.end - path.start);
    c->req->query.start = c->req->msg + (query - c->buf);
    c->req->query.len = (int)(qend - query);
    c->req->query.start[c->req->query.len] = '\0';
    c->req->body.start = c->req->msg + (hend - c->buf + 4);
    c->req->body.len = (unsigned int)clen;
    b = strstr(b, "\r\n");
    c->req->header.start = c->req->msg + (b + 2 - c->buf);
    c->req->header.len = (unsigned int)(hend - b - 2);
    c->req->res = NULL;
    c->req->hook = hook;

    return mlen;
}

static void
check_upgrade(agooCon c) {
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

bool
con_http_read(agooCon c) {
    ssize_t	cnt;

    if (c->dead || 0 == c->sock || c->closing) {
	return true;
    }
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
		log_cat(&warn_cat, "Nothing to read. Client closed socket on connection %llu.", (unsigned long long)c->id);
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
		agooReq	req;
		agooRes	res;
		long	mlen;

		if (debug_cat.on && NULL != c->req && NULL != c->req->body.start) {
		    log_cat(&debug_cat, "request on %llu: %s", (unsigned long long)c->id, c->req->body.start);
		}
		if (NULL == (res = res_create(c))) {
		    c->req = NULL;
		    log_cat(&error_cat, "memory allocation of response failed on connection %llu.", (unsigned long long)c->id);
		    return bad_request(c, 500, __LINE__);
		} else {
		    if (NULL == c->res_tail) {
			c->res_head = res;
		    } else {
			c->res_tail->next = res;
		    }
		    c->res_tail = res;
		    res->close = should_close(c->req->header.start, c->req->header.len);
		    if (res->close) {
			c->closing = true;
		    }
		}
		c->req->res = res;
		mlen = c->req->mlen;
		check_upgrade(c);
		req = c->req;
		c->req = NULL;
		if (req->hook->no_queue && FUNC_HOOK == req->hook->type) {
		    req->hook->func(req);
		} else {
		    queue_push(req->hook->queue, (void*)req);
		}
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
con_ws_read(agooCon c) {
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
		log_cat(&warn_cat, "Nothing to read. Client closed socket on connection %llu.", (unsigned long long)c->id);
	    } else {
		char	msg[1024];
		int	len = snprintf(msg, sizeof(msg) - 1, "Failed to read WebSocket message. %s.", strerror(errno));

		push_error(c->up, msg, len);
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
		if (mlen == (long)c->bcnt) {
		    ws_pong(c);
		    c->bcnt = 0;
		}
		break;
	    case WS_OP_PONG:
		// ignore
		if (mlen == (long)c->bcnt) {
		    c->bcnt = 0;
		}
		break;
	    case WS_OP_CONT:
	    default: {
		char	msg[1024];
		int	len = snprintf(msg, sizeof(msg) - 1, "WebSocket op 0x%02x not supported on %llu.",
				       op, (unsigned long long)c->id);

		push_error(c->up, msg, len);
		log_cat(&error_cat, "WebSocket op 0x%02x not supported on %llu.", op, (unsigned long long)c->id);
		return true;
	    }
	    }
	}
	if (NULL != c->req) {
	    mlen = c->req->mlen;
	    c->req->mlen = ws_decode(c->req->msg, c->req->mlen);
	    if (mlen <= (long)c->bcnt) {
		if (debug_cat.on) {
		    if (AGOO_ON_MSG == c->req->method) {
			log_cat(&debug_cat, "WebSocket message on %llu: %s", (unsigned long long)c->id, c->req->msg);
		    } else {
			log_cat(&debug_cat, "WebSocket binary message on %llu", (unsigned long long)c->id);
		    }
		}
	    }
	    upgraded_ref(c->up);
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
con_read(agooCon c) {
    if (NULL != c->bind->read) {
	return c->bind->read(c);
    }
    return true;
}

// return true to remove/close connection
bool
con_http_write(agooCon c) {
    agooText	message = res_message(c->res_head);
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
	    log_cat(&resp_cat, "%llu: %s", (unsigned long long)c->id, buf);
	}
	if (debug_cat.on) {
	    log_cat(&debug_cat, "response on %llu: %s", (unsigned long long)c->id, message->text);
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, MSG_DONTWAIT))) {
	if (EAGAIN == errno) {
	    return false;
	}
	log_cat(&error_cat, "Socket error @ %llu.", (unsigned long long)c->id);

	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	agooRes	res = c->res_head;
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
con_ws_write(agooCon c) {
    agooRes		res = c->res_head;
    agooText	message = res_message(res);
    ssize_t	cnt;

    if (NULL == message) {
	if (res->ping) {
	    if (0 > (cnt = send(c->sock, ping_msg, sizeof(ping_msg) - 1, 0))) {
		char	msg[1024];
		int	len;

		if (EAGAIN == errno) {
		    return false;
		}
		len = snprintf(msg, sizeof(msg) - 1, "Socket error @ %llu.", (unsigned long long)c->id);
		push_error(c->up, msg, len);
		
		log_cat(&error_cat, "Socket error @ %llu.", (unsigned long long)c->id);
		ws_req_close(c);
		res_destroy(res);
	
		return true;
	    }
	} else if (res->pong) {
	    if (0 > (cnt = send(c->sock, pong_msg, sizeof(pong_msg) - 1, 0))) {
		char	msg[1024];
		int	len;

		if (EAGAIN == errno) {
		    return false;
		}
		len = snprintf(msg, sizeof(msg) - 1, "Socket error @ %llu.", (unsigned long long)c->id);
		push_error(c->up, msg, len);
		log_cat(&error_cat, "Socket error @ %llu.", (unsigned long long)c->id);
		ws_req_close(c);
		res_destroy(res);
	
		return true;
	    }
	} else {
	    ws_req_close(c);
	    c->res_head = res->next;
	    if (res == c->res_tail) {
		c->res_tail = NULL;
	    }
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
	agooText	t;
	
	if (push_cat.on) {
	    if (message->bin) {
		log_cat(&push_cat, "%llu binary", (unsigned long long)c->id);
	    } else {
		log_cat(&push_cat, "%llu: %s", (unsigned long long)c->id, message->text);
	    }
	}
	t = ws_expand(message);
	if (t != message) {
	    res_set_message(res, t);
	    message = t;
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	char	msg[1024];
	int	len;

	if (EAGAIN == errno) {
	    return false;
	}
	len = snprintf(msg, sizeof(msg) - 1, "Socket error @ %llu.", (unsigned long long)c->id);
	push_error(c->up, msg, len);
	log_cat(&error_cat, "Socket error @ %llu.", (unsigned long long)c->id);
	ws_req_close(c);
	
	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	agooRes	res = c->res_head;
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

static bool
con_sse_write(agooCon c) {
    agooRes	res = c->res_head;
    agooText	message = res_message(res);
    ssize_t	cnt;

    if (NULL == message) {
	ws_req_close(c);
	res_destroy(res);
	return true;
    }
    c->timeout = dtime() + CON_TIMEOUT *2;
    if (0 == c->wcnt) {
	agooText	t;
	
	if (push_cat.on) {
	    log_cat(&push_cat, "%llu: %s", (unsigned long long)c->id, message->text);
	}
	t = sse_expand(message);
	if (t != message) {
	    res_set_message(res, t);
	    message = t;
	}
    }
    if (0 > (cnt = send(c->sock, message->text + c->wcnt, message->len - c->wcnt, 0))) {
	char	msg[1024];
	int	len;

	if (EAGAIN == errno) {
	    return false;
	}
	len = snprintf(msg, sizeof(msg) - 1, "Socket error @ %llu.", (unsigned long long)c->id);
	push_error(c->up, msg, len);
	log_cat(&error_cat, "Socket error @ %llu.", (unsigned long long)c->id);
	ws_req_close(c);
	
	return true;
    }
    c->wcnt += cnt;
    if (c->wcnt == message->len) { // finished
	agooRes	res = c->res_head;
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

static bool
con_write(agooCon c) {
    bool	remove = true;
    agooConKind	kind = c->res_head->con_kind;

    if (NULL != c->bind->write) {
	remove = c->bind->write(c);
    }
    //if (kind != c->kind && CON_ANY != kind) {
    if (CON_ANY != kind) {
	switch (kind) {
	case CON_WS:
	    c->bind = &ws_bind;
	    break;
	case CON_SSE:
	    c->bind = &sse_bind;
	    break;
	default:
	    break;
	}
    }
    return remove;
}

static void
publish_pub(agooPub pub) {
    agooUpgraded	up;
    const char		*sub = pub->subject->pattern;
    int			cnt = 0;
    
    for (up = the_server.up_list; NULL != up; up = up->next) {
	if (NULL != up->con && upgraded_match(up, sub)) {
	    agooRes	res = res_create(up->con);

	    if (NULL != res) {
		if (NULL == up->con->res_tail) {
		    up->con->res_head = res;
		} else {
		    up->con->res_tail->next = res;
		}
		up->con->res_tail = res;
		res->con_kind = CON_ANY;
		res_set_message(res, text_dup(pub->msg));
		cnt++;
	    }
	}
    }
}

static void
unsubscribe_pub(agooPub pub) {
    if (NULL == pub->up) {
	agooUpgraded	up;

	for (up = the_server.up_list; NULL != up; up = up->next) {
	    upgraded_del_subject(up, pub->subject);
	}
    } else {
	upgraded_del_subject(pub->up, pub->subject);
    }
}

static void
process_pub_con(agooPub pub) {
    agooUpgraded	up = pub->up;

    if (NULL != up) {
	int	pending;
	
	// TBD Change pending to be based on length of con queue
	if (1 == (pending = atomic_fetch_sub(&up->pending, 1))) {
	    if (NULL != up && the_server.ctx_nil_value != up->ctx && up->on_empty) {
		agooReq	req = req_create(0);
	    
		req->up = up;
		req->method = AGOO_ON_EMPTY;
		req->hook = hook_create(AGOO_NONE, NULL, up->ctx, PUSH_HOOK, &the_server.eval_queue);
		upgraded_ref(up);
		queue_push(&the_server.eval_queue, (void*)req);
	    }
	}
    }
    switch (pub->kind) {
    case PUB_CLOSE:
	// A close after already closed is used to decrement the reference
	// count on the upgraded so it can be destroyed in the con loop
	// threads.
	if (NULL != up->con) {
	    agooRes	res = res_create(up->con);

	    if (NULL != res) {
		if (NULL == up->con->res_tail) {
		    up->con->res_head = res;
		} else {
		    up->con->res_tail->next = res;
		}
		up->con->res_tail = res;
		res->con_kind = up->con->bind->kind;
		res->close = true;
	    }
	}
	break;
    case PUB_WRITE: {
	if (NULL == up->con) {
	    log_cat(&warn_cat, "Connection already closed. WebSocket write failed.");
	} else {
	    agooRes	res = res_create(up->con);

	    if (NULL != res) {
		if (NULL == up->con->res_tail) {
		    up->con->res_head = res;
		} else {
		    up->con->res_tail->next = res;
		}
		up->con->res_tail = res;
		res->con_kind = CON_ANY;
		res_set_message(res, pub->msg);
	    }
	}
	break;
    case PUB_SUB:
	upgraded_add_subject(pub->up, pub->subject);
	pub->subject = NULL;
	break;
    case PUB_UN:
	unsubscribe_pub(pub);
	break;
    case PUB_MSG:
	publish_pub(pub);
	break;
    }
    default:
	break;
    }
    pub_destroy(pub);	
}

short
con_http_events(agooCon c) {
    short	events = 0;
    
    if (NULL != c->res_head && NULL != res_message(c->res_head)) {
	events = POLLIN | POLLOUT;
    } else if (!c->closing) {
	events = POLLIN;
    }
    return events;
}

static short
con_ws_events(agooCon c) {
    short	events = 0;

    if (NULL != c->res_head && (c->res_head->close || c->res_head->ping || NULL != res_message(c->res_head))) {
	events = POLLIN | POLLOUT;
    } else if (!c->closing) {
	events = POLLIN;
    }
    return events;
}

static short
con_sse_events(agooCon c) {
    short	events = 0;

    if (NULL != c->res_head && NULL != res_message(c->res_head)) {
	events = POLLOUT;
    }
    return events;
}

static struct pollfd*
poll_setup(agooCon c, agooQueue q, struct pollfd *pp) {
    // The first two pollfd are for the con_queue and the pub_queue in that
    // order.
    pp->fd = queue_listen(&the_server.con_queue);
    pp->events = POLLIN;
    pp->revents = 0;
    pp++;
    pp->fd = queue_listen(q);
    pp->events = POLLIN;
    pp->revents = 0;
    pp++;
    for (; NULL != c; c = c->next) {
	if (c->dead || 0 == c->sock) {
	    continue;
	}
	if (c->hijacked) {
	    c->sock = 0;
	    continue;
	}
	c->pp = pp;
	pp->fd = c->sock;
	pp->events = c->bind->events(c);
	pp->revents = 0;
	pp++;
    }
    return pp;
}

static bool
remove_dead_res(agooCon c) {
    agooRes	res;

    while (NULL != (res = c->res_head)) {
	if (NULL == res_message(c->res_head) && !c->res_head->close && !c->res_head->ping) {
	    break;
	}
	c->res_head = res->next;
	if (res == c->res_tail) {
	    c->res_tail = NULL;
	}
	res_destroy(res);
    }
    return NULL == c->res_head;
}

void*
con_loop(void *x) {
    agooConLoop		loop = (agooConLoop)x;
    agooCon		c;
    agooCon		prev;
    agooCon		next;
    agooCon		cons = NULL;
    size_t		size = sizeof(struct pollfd) * INITIAL_POLL_SIZE;
    struct pollfd	*pa = (struct pollfd*)malloc(size);
    struct pollfd	*pend = pa + INITIAL_POLL_SIZE;
    struct pollfd	*pp;
    int			ccnt = 0;
    int			i;
    double		now;
    agooPub		pub;

    atomic_fetch_add(&the_server.running, 1);
    memset(pa, 0, size);
    while (the_server.active) {
	while (NULL != (c = (agooCon)queue_pop(&the_server.con_queue, 0.0))) {
	    c->next = cons;
	    cons = c;
	    ccnt++;
	    if (pend - pa < ccnt + 2) {
		size_t	cnt = (pend - pa) * 2;

		size = sizeof(struct pollfd) * cnt;
		if (NULL == (pa = (struct pollfd*)malloc(size))) {
		    log_cat(&error_cat, "Out of memory.");
		    log_close();
		    exit(EXIT_FAILURE);

		    return NULL;
		}
		pend = pa + cnt;
	    }
	}
	while (NULL != (pub = (agooPub)queue_pop(&loop->pub_queue, 0.0))) {
	    process_pub_con(pub);
	}
	pp = poll_setup(cons, &loop->pub_queue, pa);
	if (0 > (i = poll(pa, (nfds_t)(pp - pa), 10))) {
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
		while (NULL != (c = (agooCon)queue_pop(&the_server.con_queue, 0.0))) {
		    c->next = cons;
		    cons = c;
		    ccnt++;
		    if (pend - pa < ccnt + 2) {
			size_t	cnt = (pend - pa) * 2;

			size = sizeof(struct pollfd) * cnt;
			if (NULL == (pa = (struct pollfd*)malloc(size))) {
			    log_cat(&error_cat, "Out of memory.");
			    log_close();
			    exit(EXIT_FAILURE);

			    return NULL;
			}
			pend = pa + cnt;
		    }
		}
	    }
	    // Check pub_queue if an event is waiting.
	    if (0 != (pa[1].revents & POLLIN)) {
		queue_release(&loop->pub_queue);
		while (NULL != (pub = (agooPub)queue_pop(&loop->pub_queue, 0.0))) {
		    process_pub_con(pub);
		}
	    }
	}
	prev = NULL;
	for (c = cons; NULL != c; c = next) {
	    next = c->next;
	    if (0 == c->sock || NULL == c->pp) {
		continue;
	    }
	    pp = c->pp;
	    if (0 != (pp->revents & POLLIN)) {
		if (con_read(c)) {
		    c->dead = true;
		    goto CON_CHECK;
		}
	    }
	    if (0 != (pp->revents & POLLOUT)) {
		if (con_write(c)) {
		    c->dead = true;
		    goto CON_CHECK;
		}
	    }
	    if (0 != (pp->revents & (POLLERR | POLLHUP | POLLNVAL))) {
		if (0 < c->bcnt) {
		    if (0 != (pp->revents & (POLLHUP | POLLNVAL))) {
			log_cat(&error_cat, "Socket %llu closed.", (unsigned long long)c->id);
		    } else if (!c->closing) {
			log_cat(&error_cat, "Socket %llu error. %s", (unsigned long long)c->id, strerror(errno));
		    }
		}
		c->dead = true;
		goto CON_CHECK;
	    }
	CON_CHECK:
	    if (c->dead || 0 == c->sock) {
		if (remove_dead_res(c)) {
		    goto CON_RM;
		}
	    } else if (0.0 == c->timeout || now < c->timeout) {
		prev = c;
		continue;
	    } else if (c->closing) {
		if (remove_dead_res(c)) {
		    goto CON_RM;
		}
	    } else if (CON_WS == c->bind->kind || CON_SSE == c->bind->kind) {
		c->timeout = dtime() + CON_TIMEOUT;
		ws_ping(c);
		continue;
	    } else {
		c->closing = true;
		c->timeout = now + 0.5;
		prev = c;
		continue;
	    }
	    prev = c;
	    continue;
	CON_RM:
	    if (NULL == prev) {
		cons = next;
	    } else {
		prev->next = next;
	    }
	    ccnt--;
	    log_cat(&con_cat, "Connection %llu closed.", (unsigned long long)c->id);
	    con_destroy(c);
	}
    }
    while (NULL != (c = cons)) {
	cons = c->next;
	con_destroy(c);
    }
    atomic_fetch_sub(&the_server.running, 1);
    
    return NULL;
}

agooConLoop
conloop_create(agooErr err, int id) {
     agooConLoop	loop;

    if (NULL == (loop = (agooConLoop)malloc(sizeof(struct _agooConLoop)))) {
	err_set(err, ERR_MEMORY, "Failed to allocate memory for a connection thread.");
    } else {
	//DEBUG_ALLOC(mem_con, c);
	loop->next = NULL;
	queue_multi_init(&loop->pub_queue, 256, true, false);
	loop->id = id;
	pthread_create(&loop->thread, NULL, con_loop, loop);
    }
    return loop;
}
