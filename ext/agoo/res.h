// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RES_H
#define AGOO_RES_H

#include <stdbool.h>

#include "atomic.h"
#include "con.h"
#include "text.h"

struct _agooCon;

typedef struct _agooRes {
    struct _agooRes	*next;
    struct _agooCon	*con;
    _Atomic(agooText)	message;
    agooConKind		con_kind;
    bool		close;
    bool		ping;
    bool		pong;
} *agooRes;

extern agooRes	agoo_res_create(struct _agooCon *con);
extern void	agoo_res_destroy(agooRes res);
extern void	agoo_res_set_message(agooRes res, agooText t);

static inline agooText
agoo_res_message(agooRes res) {
    return atomic_load(&res->message);
}

#endif // AGOO_RES_H
