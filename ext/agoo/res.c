// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "res.h"

Res
res_create() {
    Res	res = (Res)malloc(sizeof(struct _Res));

    if (NULL != res) {
	res->next = NULL;
	atomic_init(&res->message, NULL);
	res->close = false;
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

