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

    // Link: </style.css>; rel=preload; as=style
    cnt = (int)RARRAY_LEN(links);
    for (i = cnt - 1; 0 <= i; i--) {
	lv = rb_ary_entry(links, i);
	if (2 != RARRAY_LEN(lv)) {
	    rb_raise(rb_eArgError, "early hints call argument must be an array of arrays that have 2 string members.");
	}
	v = rb_ary_entry(lv, 0);
	key = rb_string_value_ptr((VALUE*)&v);
	if (0 != strcmp("Link", key)) {
	    rb_raise(rb_eArgError, "Only a 'Link' header name is allowed in early hints. '%s' is not allowed", key);
	}
	v = rb_ary_entry(lv, 1);
	content = rb_string_value_ptr((VALUE*)&v);
	if (NULL == (link = agoo_early_create(content))) {
	    rb_raise(rb_eNoMemError, "out of memory");
	}
	link->next = ll;
	ll = link;
    }
    agoo_res_add_early(eh->req->res, ll);

    return Qnil;
}

static int
size_cb(VALUE key, VALUE value, int *sizep) {
    switch (rb_type(key)) {
    case RUBY_T_STRING:
	*sizep += (int)RSTRING_LEN(key);
	break;
    case RUBY_T_SYMBOL: {
	volatile VALUE	v = rb_sym_to_s(key);

	*sizep += (int)RSTRING_LEN(v);
	break;
    }
    default:
	rb_raise(rb_eArgError, "early link push options key must be a Symbol or String");
	break;
    }
    rb_check_type(value, RUBY_T_STRING);
    *sizep += (int)RSTRING_LEN(value);
    *sizep += 3;

    return ST_CONTINUE;
}

static int
append_options_cb(VALUE key, VALUE value, agooEarly e) {
    const char		*s;
    char		*end;
    int			len;
    volatile VALUE	v;

    for (end = e->link; '\0' != *end; end++) {
    }
    *end++ = ' ';
    switch (rb_type(key)) {
    case RUBY_T_STRING:
	s = rb_string_value_ptr((VALUE*)&key);
	len = (int)RSTRING_LEN(key);
	break;
    case RUBY_T_SYMBOL:
	v = rb_sym_to_s(key);
	s = rb_string_value_ptr((VALUE*)&v);
	len = (int)RSTRING_LEN(v);
	break;
    default:
	rb_raise(rb_eArgError, "early link push options key must be a Symbol or String");
	break;
    }
    strncpy(end, s, len);
    end += len;
    *end++ = '=';

    rb_check_type(value, RUBY_T_STRING);
    s = rb_string_value_ptr((VALUE*)&value);
    len = (int)RSTRING_LEN(value);

    strncpy(end, s, len);
    end += len;
    *end++ = ';';

    return ST_CONTINUE;
}

static int
push_cb(VALUE key, VALUE value, EarlyHints eh) {
    const char	*path = rb_string_value_ptr((VALUE*)&key);
    int		plen = (int)RSTRING_LEN(key);
    int		size = plen + 5; // </>;\0
    agooEarly	link;
    char	*end;

    rb_check_type(value, RUBY_T_HASH);
    rb_hash_foreach(value, size_cb, (VALUE)&size);

    if (NULL == (link = agoo_early_alloc(size))) {
	rb_raise(rb_eNoMemError, "out of memory");
    }
    end = link->link;
    *end++ = '<';
    strncpy(end, path, plen);
    end += plen;
    *end++ = '>';
    *end++ = ';';
    rb_hash_foreach(value, append_options_cb, (VALUE)link);

    link->next = eh->links;
    eh->links = link;

    return ST_CONTINUE;
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
 *                       "/style.css" => { rel: 'preload', as: 'style' },
 *                       "/scrips.js" => { rel: 'preload', as: 'script' },
 *                     })
 */
static VALUE
eh_push(VALUE self, VALUE links) {
    EarlyHints	eh = (EarlyHints)DATA_PTR(self);

    if (NULL == eh) {
	rb_raise(rb_eIOError, "early hints has been closed.");
    }
    eh->links = NULL;
    rb_hash_foreach(links, push_cb, (VALUE)eh);
    agoo_res_add_early(eh->req->res, eh->links);
    eh->links = NULL;

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
