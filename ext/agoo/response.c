// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "http.h"
#include "response.h"

int
response_len(agooResponse res) {
    char	buf[256];
    const char *msg = http_code_message(res->code);
    int		len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n", res->code, msg, res->blen);
    agooHeader	h;

    for (h = res->headers; NULL != h; h = h->next) {
	len += h->len;
    }
    len += 2; // for additional \r\n before body
    len += res->blen;
    
    return len;
}

void
response_fill(agooResponse res, char *buf) {
    agooHeader	h;
    const char *msg = http_code_message(res->code);

    buf += sprintf(buf, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n", res->code, msg, res->blen);

    for (h = res->headers; NULL != h; h = h->next) {
	strncpy(buf, h->text, h->len);
	buf += h->len;
    }
    *buf++ = '\r';
    *buf++ = '\n';
    if (NULL != res->body) {
	memcpy(buf, res->body, res->blen);
	buf += res->blen;
    }
    *buf = '\0';
}
