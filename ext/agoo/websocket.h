// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_WEBSOCKET_H
#define AGOO_WEBSOCKET_H

#define WS_OP_CONT	0x00
#define WS_OP_TEXT	0x01
#define WS_OP_BIN	0x02
#define WS_OP_CLOSE	0x08
#define WS_OP_PING	0x09
#define WS_OP_PONG	0x0A

struct _agooReq;
struct _agooText;

extern struct _agooText*	ws_add_headers(struct _agooReq *req, struct _agooText *t);
extern struct _agooText*	ws_expand(agooText t);
extern size_t			ws_decode(char *buf, size_t mlen);

extern long			ws_calc_len(agooCon c, uint8_t *buf, size_t cnt);
extern bool			ws_create_req(agooCon c, long mlen);
extern void			ws_req_close(agooCon c);

extern void			ws_ping(agooCon c);
extern void			ws_pong(agooCon c);

#endif // AGOO_WEBSOCKET_H
