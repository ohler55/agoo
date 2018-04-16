// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_WEBSOCKET_H__
#define __AGOO_WEBSOCKET_H__

struct _Req;
struct _Text;

extern struct _Text*	ws_add_headers(struct _Req *req, struct _Text *t);
extern Text		ws_expand(Text t);

#endif // __AGOO_WEBSOCKET_H__
