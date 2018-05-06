// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "con.h"
#include "debug.h"
#include "res.h"

Res
res_create(Con con) {
    Res	res = (Res)malloc(sizeof(struct _Res));

    if (NULL != res) {
	DEBUG_ALLOC(mem_res, res)
	res->next = NULL;
	atomic_init(&res->message, NULL);
	res->con = con;
	res->con_kind = CON_HTTP;
	res->close = false;
	res->ping = false;
	res->pong = false;
    }
    return res;
}

void
res_destroy(Res res) {
    if (NULL != res) {
	Text	message = res_message(res);

	if (NULL != message) {
	    text_release(message);
	}
	DEBUG_FREE(mem_res, res)
	free(res);
    }
}

void
res_set_message(Res res, Text t) {
    if (NULL != t) {
	text_ref(t);
    }
    atomic_store(&res->message, t);
}

