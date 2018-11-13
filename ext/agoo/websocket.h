// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_WEBSOCKET_H
#define AGOO_WEBSOCKET_H

#define WS_OP_CONT	0x00
#define WS_OP_TEXT	0x01
#define WS_OP_BIN	0x02
#define WS_OP_CLOSE	0x08
#define WS_OP_PING	0x09
#define WS_OP_PONG	0x0A

struct _Req;
struct _Text;

extern struct _Text*	ws_add_headers(struct _Req *req, struct _Text *t);
extern Text		ws_expand(Text t);
extern size_t		ws_decode(char *buf, size_t mlen);

extern long		ws_calc_len(Con c, uint8_t *buf, size_t cnt);
extern bool		ws_create_req(Con c, long mlen);
extern void		ws_req_close(Con c);

extern void		ws_ping(Con c);
extern void		ws_pong(Con c);

#endif // AGOO_WEBSOCKET_H
