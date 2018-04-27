// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_SSE_H__
#define __AGOO_SSE_H__

struct _Req;
struct _Text;

extern struct _Text*	sse_upgrade(struct _Req *req, struct _Text *t);
extern struct _Text*	sse_expand(struct _Text *t);

#endif // __AGOO_SSE_H__
