// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdarg.h>
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
    res->message = NULL;
    pthread_mutex_init(&res->lock, NULL);
    res->con = con;
    res->con_kind = AGOO_CON_HTTP;
    res->final = false;
    res->close = false;
    res->ping = false;
    res->pong = false;

    return res;
}

void
agoo_res_destroy(agooRes res) {
    if (NULL != res) {
	if (NULL != res->message) {
	    agoo_text_release(res->message);
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
agoo_res_message_push(agooRes res, agooText t, bool final) {
    if (NULL != t) {
	agoo_text_ref(t);
    }
    pthread_mutex_lock(&res->lock);
    if (!res->final) {
	if (NULL == res->message) {
	    res->message = t;
	} else {
	    agooText	end = res->message;

	    for (; NULL != end->next; end = end->next) {
	    }
	    end->next = t;
	}
	res->final = final;
    }
    pthread_mutex_unlock(&res->lock);
}

static const char	early_103[] = "HTTP/1.1 103 Early Hints\r\n";

void
agoo_res_add_early(agooRes res, agooEarly early) {
    agooText	t = agoo_text_allocate(1024);

    t = agoo_text_append(t, early_103, sizeof(early_103) - 1);
    for (; NULL != early; early = early->next) {
	t = agoo_text_append(t, "Link: ", 6);
	t = agoo_text_append(t, early->link, -1);
	t = agoo_text_append(t, "\r\n", 2);
    }
    t = agoo_text_append(t, "\r\n", 2);
    agoo_res_message_push(res, t, false);
}

agooText
agoo_res_message_peek(agooRes res) {
    agooText	t;

    pthread_mutex_lock(&res->lock);
    t = res->message;
    pthread_mutex_unlock(&res->lock);

    return t;
}

agooText
agoo_res_message_next(agooRes res) {
    agooText	t;

    pthread_mutex_lock(&res->lock);
    if (NULL != res->message) {
	agooText	t2 = res->message;

	res->message = res->message->next;
	// TBD make sure it is not release by the called, change code if it is
	agoo_text_release(t2);
    }
    t = res->message;
    pthread_mutex_unlock(&res->lock);

    return t;
}
