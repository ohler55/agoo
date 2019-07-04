// Copyright (c) 2019, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "debug.h"
#include "early_hints.h"
#include "req.h"

static VALUE	eh_class = Qundef;

typedef struct _earlyHints {
    agooReq	req;
    bool	called;
} *EarlyHints;

static void
eh_free(void *ptr) {
    if (NULL != ptr) {
	EarlyHints	eh = (EarlyHints)ptr;

	// TBD free stored links

	AGOO_FREE(ptr);
    }
}

VALUE
agoo_early_hints_create(agooReq req) {
    EarlyHints	eh = (EarlyHints)AGOO_MALLOC(sizeof(struct _earlyHints));

    if (NULL == eh) {
	rb_raise(rb_eNoMemError, "Failed to allocate memory for an early hints object.");
    }
    eh->req = req;
    eh->called = false;

    return Data_Wrap_Struct(eh_class, NULL, eh_free, eh);
}

/* Document-method: call
 *
 * call-seq: call(links)
 *
 * Write early hints, response with status 103 using the provided headers in
 * an Array of Array.
 *
 * Example:
 *
 *    early_hints.call([
 *                      ["link", "</style.css>; rel=preload; as=style"],
 *                      ["link", "</script.js>; rel=preload; as=script"],
 *                     ])
 */
static VALUE
eh_call(VALUE self, VALUE links) {
    EarlyHints	eh = (EarlyHints)DATA_PTR(self);

    if (NULL == eh) {
	rb_raise(rb_eIOError, "early hints has been closed.");
    }
    if (eh->called) {
	rb_raise(rb_eIOError, "early hints can only be called once.");
    }
    eh->called = true;

    printf("*** early hints call()\n");

    // TBD form header lines, links is an array, store to be used for final response, then write

    return Qnil;
}

/* Document-method: push
 *
 * call-seq: push(links)
 *
 * Write early hints, response with status 103 using the provided links in a
 * Hash.
 *
 * Example:
 *
 *    early_hints.push({
 *                       "/style.css" => { preload: true, rel: 'style' },
 *                       "/scrips.js" => { preload: true, rel: 'script' },
 *                     })
 */
static VALUE
eh_push(VALUE self, VALUE path, VALUE options) {
    EarlyHints	eh = (EarlyHints)DATA_PTR(self);

    if (NULL == eh) {
	rb_raise(rb_eIOError, "early hints has been closed.");
    }
    if (eh->called) {
	rb_raise(rb_eIOError, "early hints can only be called once.");
    }
    eh->called = true;

    printf("*** early hints push()\n");

    // TBD form header lines, links is a hash, store to be used for final response, then write

    return Qnil;
}

/* Document-class: Agoo::EarlyHints
 *
 * Used to provide early hints (HTTP 103) responses to requests.
 */
void
early_hints_init(VALUE mod) {
    eh_class = rb_define_class_under(mod, "EarlyHints", rb_cObject);

    rb_define_method(eh_class, "call", eh_call, 1);
    rb_define_method(eh_class, "push", eh_push, 1);
}
