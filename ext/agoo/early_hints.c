// Copyright (c) 2019, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "debug.h"
#include "early.h"
#include "early_hints.h"
#include "req.h"
#include "res.h"

static VALUE	eh_class = Qundef;

typedef struct _earlyHints {
    agooReq	req;
    agooEarly	links;
} *EarlyHints;

static void
eh_free(void *ptr) {
    if (NULL != ptr) {
	AGOO_FREE(ptr);
    }
}

VALUE
agoo_early_hints_new(agooReq req) {
    EarlyHints	eh = (EarlyHints)AGOO_CALLOC(1, sizeof(struct _earlyHints));

    if (NULL == eh) {
	rb_raise(rb_eNoMemError, "Failed to allocate memory for an early hints object.");
    }
    eh->req = req;

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
    agooEarly	link;
    agooEarly	ll = NULL;
    int		i, cnt;
    VALUE	lv;
    VALUE	v;
    const char	*key;
    const char	*content;

    if (NULL == eh) {
	rb_raise(rb_eIOError, "early hints has been closed.");
    }
    rb_check_type(links, RUBY_T_ARRAY);

    // Link: </style.css>; rel=preload; as=style
    cnt = (int)RARRAY_LEN(links);
    for (i = cnt - 1; 0 <= i; i--) {
	lv = rb_ary_entry(links, i);
	switch (rb_type(lv)) {
	case RUBY_T_STRING:
	    content = rb_string_value_ptr((VALUE*)&v);
	    if (NULL == (link = agoo_early_create(content))) {
		rb_raise(rb_eNoMemError, "out of memory");
	    }
	    break;
	case RUBY_T_ARRAY:
	    if (2 != RARRAY_LEN(lv)) {
		rb_raise(rb_eArgError, "early hints call argument must be an array of arrays that have 2 string members.");
	    }
	    v = rb_ary_entry(lv, 0);
	    key = rb_string_value_ptr((VALUE*)&v);
	    if (0 != strcasecmp("link", key)) {
		rb_raise(rb_eArgError, "Only a 'Link' header is allowed in early hints. '%s' is not allowed", key);
	    }
	    v = rb_ary_entry(lv, 1);
	    content = rb_string_value_ptr((VALUE*)&v);
	    if (NULL == (link = agoo_early_create(content))) {
		rb_raise(rb_eNoMemError, "out of memory");
	    }
	    break;
	default:
	    rb_raise(rb_eArgError, "early hints call argument must be an array of arrays or an array of strings.");
	}
	link->next = ll;
	ll = link;
    }
    agoo_res_add_early(eh->req->res, ll);
    agoo_early_destroy(ll);

    return Qnil;
}

/* Document-class: Agoo::EarlyHints
 *
 * Used to provide early hints (HTTP 103) responses to requests.
 */
void
early_hints_init(VALUE mod) {
    eh_class = rb_define_class_under(mod, "EarlyHints", rb_cObject);

    rb_undef_alloc_func(eh_class);
    rb_define_method(eh_class, "call", eh_call, 1);
}
