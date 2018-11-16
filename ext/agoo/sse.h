// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SSE_H
#define AGOO_SSE_H

struct _agooReq;
struct _agooText;

extern struct _agooText*	sse_upgrade(struct _agooReq *req, struct _agooText *t);
extern struct _agooText*	sse_expand(struct _agooText *t);

#endif // AGOO_SSE_H
