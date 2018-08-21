// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "req.h"
#include "sse.h"
#include "text.h"

static const char	prefix[] = "event: msg\ndata: ";
static const char	suffix[] = "\n\n";
static const char	up[] = "HTTP/1.1 200 OK\r\n\
Content-Type: text/event-stream\r\n\
Cache-Control: no-cache\r\n\
Connection: keep-alive\r\n\
\r\n\
retry: 5\n\n";

Text
sse_upgrade(Req req, Text t) {
    t->len = 0; // reset
    return text_append(t, up, sizeof(up) - 1);
}

Text
sse_expand(Text t) {
    t = text_prepend(t, prefix, sizeof(prefix) - 1);
    return text_append(t, suffix, sizeof(suffix) - 1);
}
