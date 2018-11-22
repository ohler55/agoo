// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdint.h>
#include <string.h>

#include "base64.h"
#include "con.h"
#include "debug.h"
#include "log.h"
#include "req.h"
#include "res.h"
#include "sha1.h"
#include "server.h"
#include "text.h"
#include "upgraded.h"
#include "websocket.h"

#define MAX_KEY_LEN	1024

static const char	up_con[] = "Upgrade: websocket\r\nConnection: Upgrade\r\n";
static const char	ws_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char	ws_protocol[] = "Sec-WebSocket-Protocol: ";
static const char	ws_accept[] = "Sec-WebSocket-Accept: ";

//static const uint8_t	close_msg[] = "\x88\x02\x03\xE8";

agooText
agoo_ws_add_headers(agooReq req, agooText t) {
    int		klen = 0;
    const char	*key;
    
    t = agoo_text_append(t, up_con, sizeof(up_con) - 1);
    if (NULL != (key = agoo_con_header_value(req->header.start, req->header.len, "Sec-WebSocket-Key", &klen)) &&
	klen + sizeof(ws_magic) < MAX_KEY_LEN) {
	char		buf[MAX_KEY_LEN];
	unsigned char	sha[32];
	int		len = klen + sizeof(ws_magic) - 1;

	strncpy(buf, key, klen);
	strcpy(buf + klen, ws_magic);
	sha1((unsigned char*)buf, len, sha);
	len = b64_to(sha, SHA1_DIGEST_SIZE, buf);

	t = agoo_text_append(t, ws_accept, sizeof(ws_accept) - 1);
	t = agoo_text_append(t, buf, len);
	t = agoo_text_append(t, "\r\n", 2);
    }
    if (NULL != (key = agoo_con_header_value(req->header.start, req->header.len, "Sec-WebSocket-Protocol", &klen))) {
	t = agoo_text_append(t, ws_protocol, sizeof(ws_protocol) - 1);
	t = agoo_text_append(t, key, klen);
	t = agoo_text_append(t, "\r\n", 2);
    }
    return t;
}

agooText
agoo_ws_expand(agooText t) {
    uint8_t	buf[16];
    uint8_t	*b = buf;
    uint8_t	opcode = t->bin ? AGOO_WS_OP_BIN : AGOO_WS_OP_TEXT;

    *b++ = 0x80 | (uint8_t)opcode;
    // send unmasked
    if (125 >= t->len) {
	*b++ = (uint8_t)t->len;
    } else if (0xFFFF >= t->len) {
	*b++ = (uint8_t)0x7E;
	*b++ = (uint8_t)((t->len >> 8) & 0xFF);
	*b++ = (uint8_t)(t->len & 0xFF);
    } else {
	int	i;

	*b++ = (uint8_t)0x7F;
	for (i = 56; 0 <= i; i -= 8) {
	    *b++ = (uint8_t)((t->len >> i) & 0xFF);
	}
    }
    return agoo_text_prepend(t, (const char*)buf, (int)(b - buf));
}

size_t
agoo_ws_decode(char *buf, size_t mlen) {
    uint8_t	*b = (uint8_t*)buf;
    bool	is_masked;
    uint8_t	*payload;
    uint64_t	plen;
    
    b++; // op
    is_masked = 0x80 & *b;
    plen = 0x7F & *b;
    b++;
    if (126 == plen) {
	plen = *b++;
	plen = (plen << 8) | *b++;
    } else if (127 == plen) {
	plen = *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
    }
    if (is_masked) {
	uint8_t		mask[4];
	uint64_t	i;
	
	for (i = 0; i < 4; i++, b++) {
	    mask[i] = *b;
	}
	payload = b;
	for (i = 0; i < plen; i++, b++) {
	    *b = *b ^ mask[i % 4];
	}
    } else {
	payload = b;
	b += plen;
    }
    memmove(buf, payload, plen);
    buf[plen] = '\0';

    return plen;
}

// if -1 then err, 0 not ready yet, positive is completed length
long
agoo_ws_calc_len(agooCon c, uint8_t *buf, size_t cnt) {
    uint8_t	*b = buf;
    bool	is_masked;
    uint64_t	plen;

    if (0 == (0x80 & *b)) {
	agoo_log_cat(&agoo_error_cat, "FIN must be 1. Websocket continuation not implemented on connection %llu.", (unsigned long long)c->id);
	return -1;
    }
    b++;
    is_masked = 0x80 & *b;
    plen = 0x7F & *b;
    if (cnt < 3) {
	return 0; // not read yet
    }
    b++;
    if (126 == plen) {
	if (cnt < 5) {
	    return 0; // not read yet
	}
	plen = *b++;
	plen = (plen << 8) | *b++;
    } else if (127 == plen) {
	if (c->bcnt < 11) {
	    return 0; // not read yet
	}
	plen = *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
	plen = (plen << 8) | *b++;
    }
    return (long)(b - buf) + (is_masked ? 4 : 0) + plen;
}

// Return true on error otherwise false.
bool
agoo_ws_create_req(agooCon c, long mlen) {
    uint8_t	op = 0x0F & *c->buf;
    
    if (NULL == (c->req = agoo_req_create(mlen))) {
	agoo_log_cat(&agoo_error_cat, "Out of memory attempting to allocate request.");
	return true;
    }
    if (NULL == c->up || agoo_server.ctx_nil_value == c->up->ctx) {
	return true;
    }
    memset(c->req, 0, sizeof(struct _agooReq));
    if ((long)c->bcnt <= mlen) {
	memcpy(c->req->msg, c->buf, c->bcnt);
	if ((long)c->bcnt < mlen) {
	    memset(c->req->msg + c->bcnt, 0, mlen - c->bcnt);
	}
    } else {
	memcpy(c->req->msg, c->buf, mlen);
    }
    c->req->msg[mlen] = '\0';
    c->req->mlen = mlen;
    c->req->method = (AGOO_WS_OP_BIN == op) ? AGOO_ON_BIN : AGOO_ON_MSG;
    c->req->upgrade = AGOO_UP_NONE;
    c->req->up = c->up;
    c->req->res = NULL;
    if (c->up->on_msg) {
	c->req->hook = agoo_hook_create(AGOO_NONE, NULL, c->up->ctx, PUSH_HOOK, &agoo_server.eval_queue);
    }
    return false;
}

void
agoo_ws_req_close(agooCon c) {
    if (NULL != c->up && agoo_server.ctx_nil_value != c->up->ctx && c->up->on_close) {
	agooReq	req = agoo_req_create(0);
	    
	req->up = c->up;
	req->method = AGOO_ON_CLOSE;
	req->hook = agoo_hook_create(AGOO_NONE, NULL, c->up->ctx, PUSH_HOOK, &agoo_server.eval_queue);
	atomic_fetch_add(&c->up->ref_cnt, 1);
	agoo_queue_push(&agoo_server.eval_queue, (void*)req);
    }
}

void
agoo_ws_ping(agooCon c) {
    agooRes	res;
    
    if (NULL == (res = agoo_res_create(c))) {
	agoo_log_cat(&agoo_error_cat, "Memory allocation of response failed on connection %llu.", (unsigned long long)c->id);
    } else {
	if (NULL == c->res_tail) {
	    c->res_head = res;
	} else {
	    c->res_tail->next = res;
	}
	c->res_tail = res;
	res->close = false;
	res->con_kind = AGOO_CON_WS;
	res->ping = true;
    }
}

void
agoo_ws_pong(agooCon c) {
    agooRes	res;
    
    if (NULL == (res = agoo_res_create(c))) {
	agoo_log_cat(&agoo_error_cat, "Memory allocation of response failed on connection %llu.", (unsigned long long)c->id);
    } else {
	if (NULL == c->res_tail) {
	    c->res_head = res;
	} else {
	    c->res_tail->next = res;
	}
	c->res_tail = res;
	res->close = false;
	res->con_kind = AGOO_CON_WS;
	res->pong = true;
    }
}
