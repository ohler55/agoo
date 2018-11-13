// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RES_H
#define AGOO_RES_H

#include <stdatomic.h>
#include <stdbool.h>

#include "text.h"
#include "con.h"

struct _Con;

typedef struct _Res {
    struct _Res		*next;
    struct _Con		*con;
    _Atomic(Text)	message;
    ConKind		con_kind;
    bool		close;
    bool		ping;
    bool		pong;
} *Res;

extern Res	res_create(struct _Con *con);
extern void	res_destroy(Res res);
extern void	res_set_message(Res res, Text t);

static inline Text
res_message(Res res) {
    return atomic_load(&res->message);
}

#endif // AGOO_RES_H
