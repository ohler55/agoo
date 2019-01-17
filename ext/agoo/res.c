// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>

#include "con.h"
#include "debug.h"
#include "res.h"

agooRes
agoo_res_create(agooCon con) {
    agooRes	res = NULL;

    pthread_mutex_lock(&con->loop->lock);
    if (NULL != (res = con->loop->res_head)) {
	con->loop->res_head = res->next;
	if (NULL == con->loop->res_head) {
	    con->loop->res_tail = NULL;
	}
    }
    pthread_mutex_unlock(&con->loop->lock);

    if (NULL == res) {
	if (NULL == (res = (agooRes)AGOO_MALLOC(sizeof(struct _agooRes)))) {
	    return NULL;
	}
    }
    res->next = NULL;
    atomic_init(&res->message, NULL);
    res->con = con;
    res->con_kind = AGOO_CON_HTTP;
    res->close = false;
    res->ping = false;
    res->pong = false;

    return res;
}

void
agoo_res_destroy(agooRes res) {
    if (NULL != res) {
	agooText	message = agoo_res_message(res);

	if (NULL != message) {
	    agoo_text_release(message);
	}
	res->next = NULL;
	pthread_mutex_lock(&res->con->loop->lock);
	if (NULL == res->con->loop->res_tail) {
	    res->con->loop->res_head = res;
	} else {
	    res->con->loop->res_tail->next = res;
	}
	res->con->loop->res_tail = res;
	pthread_mutex_unlock(&res->con->loop->lock);
    }
}

void
agoo_res_set_message(agooRes res, agooText t) {
    if (NULL != t) {
	agoo_text_ref(t);
    }
    atomic_store(&res->message, t);
}

