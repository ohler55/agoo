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

extern agooRes	res_create(struct _agooCon *con);
extern void	res_destroy(agooRes res);
extern void	res_set_message(agooRes res, agooText t);

static inline agooText
res_message(agooRes res) {
    return atomic_load(&res->message);
}

#endif // AGOO_RES_H
