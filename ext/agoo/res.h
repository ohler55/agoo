// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RES_H
#define AGOO_RES_H

#include <pthread.h>
#include <stdbool.h>

#include "atomic.h"
#include "con.h"
#include "early.h"
#include "text.h"

struct _agooCon;

typedef struct _agooRes {
    struct _agooRes	*next;
    struct _agooCon	*con;
    volatile agooText	message;
    pthread_mutex_t	lock; // a lock around message changes
    volatile bool	final;
    agooConKind		con_kind;
    bool		close;
    bool		ping;
    bool		pong;
} *agooRes;

extern agooRes		agoo_res_create(struct _agooCon *con);
extern void		agoo_res_destroy(agooRes res);

extern void		agoo_res_message_push(agooRes res, agooText t);
extern void		agoo_res_add_early(agooRes res, agooEarly early);
extern agooText		agoo_res_message_peek(agooRes res);
extern agooText		agoo_res_message_next(agooRes res);

#endif // AGOO_RES_H
