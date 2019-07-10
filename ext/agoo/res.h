// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RES_H
#define AGOO_RES_H

#include <stdbool.h>

#include "atomic.h"
#include "con.h"
#include "early.h"
#include "text.h"

struct _agooCon;

typedef struct _agooRes {
    struct _agooRes	*next;
    struct _agooCon	*con;
    _Atomic(agooText)	message;
    _Atomic(agooEarly)	early;
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

static inline void
agoo_res_add_early(agooRes res, agooEarly early) {
    atomic_store(&res->early, early);
}

#endif // AGOO_RES_H
