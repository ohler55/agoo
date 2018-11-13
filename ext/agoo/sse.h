// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SSE_H
#define AGOO_SSE_H

struct _Req;
struct _Text;

extern struct _Text*	sse_upgrade(struct _Req *req, struct _Text *t);
extern struct _Text*	sse_expand(struct _Text *t);

#endif // AGOO_SSE_H
