// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdint.h>

#include "base64.h"
#include "con.h"
#include "request.h"
#include "sha1.h"
#include "text.h"
#include "websocket.h"

#define MAX_KEY_LEN	1024

#define OP_CONT		0x00
#define OP_TEXT		0x01
#define OP_BIN		0x02
#define OP_CLOSE	0x08
#define OP_PING		0x09
#define OP_PONG		0x0A

static const char	up_con[] = "Upgrade: websocket\r\nConnection: Upgrade\r\n";
static const char	ws_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char	ws_protocol[] = "Sec-WebSocket-Protocol: ";
static const char	ws_accept[] = "Sec-WebSocket-Accept: ";

//static const uint8_t	close_msg[] = "\x88\x02\x03\xE8";
//static const uint8_t	pong_msg[] = "\x8A\x00";

Text
ws_add_headers(Req req, Text t) {
    int		klen = 0;
    const char	*key;
    
    t = text_append(t, up_con, sizeof(up_con) - 1);
    if (NULL != (key = con_header_value(req->header.start, req->header.len, "Sec-WebSocket-Key", &klen)) &&
	klen + sizeof(ws_magic) < MAX_KEY_LEN) {
	char		buf[MAX_KEY_LEN];
	unsigned char	sha[32];
	int		len = klen + sizeof(ws_magic) - 1;

	strncpy(buf, key, klen);
	strcpy(buf + klen, ws_magic);
	sha1((unsigned char*)buf, len, sha);
	len = b64_to(sha, SHA1_DIGEST_SIZE, buf);

	t = text_append(t, ws_accept, sizeof(ws_accept) - 1);
	t = text_append(t, buf, len);
	t = text_append(t, "\r\n", 2);
    }
    if (NULL != (key = con_header_value(req->header.start, req->header.len, "Sec-WebSocket-Protocol", &klen))) {
	t = text_append(t, ws_protocol, sizeof(ws_protocol) - 1);
	t = text_append(t, key, klen);
	t = text_append(t, "\r\n", 2);
    }
    return t;
}

Text
ws_expand(Text t) {
    uint8_t	buf[16];
    uint8_t	*b = buf;
    uint8_t	opcode = t->bin ? OP_BIN : OP_TEXT;

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
    return text_prepend(t, (const char*)buf, b - buf);
}
