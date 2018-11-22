// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_WEBSOCKET_H
#define AGOO_WEBSOCKET_H

#define AGOO_WS_OP_CONT		0x00
#define AGOO_WS_OP_TEXT		0x01
#define AGOO_WS_OP_BIN		0x02
#define AGOO_WS_OP_CLOSE	0x08
#define AGOO_WS_OP_PING		0x09
#define AGOO_WS_OP_PONG		0x0A

struct _agooReq;
struct _agooText;

extern struct _agooText*	agoo_ws_add_headers(struct _agooReq *req, struct _agooText *t);
extern struct _agooText*	agoo_ws_expand(agooText t);
extern size_t			agoo_ws_decode(char *buf, size_t mlen);

extern long			agoo_ws_calc_len(agooCon c, uint8_t *buf, size_t cnt);
extern bool			agoo_ws_create_req(agooCon c, long mlen);
extern void			agoo_ws_req_close(agooCon c);

extern void			agoo_ws_ping(agooCon c);
extern void			agoo_ws_pong(agooCon c);

#endif // AGOO_WEBSOCKET_H
