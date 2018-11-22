// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "con.h"
#include "debug.h"
#include "res.h"

agooRes
agoo_res_create(agooCon con) {
    agooRes	res = (agooRes)malloc(sizeof(struct _agooRes));

    if (NULL != res) {
	DEBUG_ALLOC(mem_res, res)
	res->next = NULL;
	atomic_init(&res->message, NULL);
	res->con = con;
	res->con_kind = AGOO_CON_HTTP;
	res->close = false;
	res->ping = false;
	res->pong = false;
    }
    return res;
}

void
agoo_res_destroy(agooRes res) {
    if (NULL != res) {
	agooText	message = agoo_res_message(res);

	if (NULL != message) {
	    agoo_text_release(message);
	}
	DEBUG_FREE(mem_res, res)
	free(res);
    }
}

void
agoo_res_set_message(agooRes res, agooText t) {
    if (NULL != t) {
	agoo_text_ref(t);
    }
    atomic_store(&res->message, t);
}

