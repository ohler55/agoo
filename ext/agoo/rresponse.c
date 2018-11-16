// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "debug.h"
#include "http.h"
#include "response.h"
#include "rresponse.h"
#include "text.h"

static VALUE	res_class = Qundef;

static void
response_free(void *ptr) {
    agooResponse	res = (agooResponse)ptr;
    agooHeader		h;

    while (NULL != (h = res->headers)) {
	res->headers = h->next;
	DEBUG_FREE(mem_header, h);
	xfree(h);
    }
    DEBUG_FREE(mem_res_body, res->body);
    DEBUG_FREE(mem_response, ptr);
    free(res->body); // allocated with strdup
    xfree(ptr);
}

VALUE
response_new( ) {
    agooResponse	res = ALLOC(struct _agooResponse);

    DEBUG_ALLOC(mem_response, res)
    memset(res, 0, sizeof(struct _agooResponse));
    res->code = 200;
    
    return Data_Wrap_Struct(res_class, NULL, response_free, res);
}

/* Document-method: to_s
 *
 * call-seq: to_s()
 *
 * Returns a string representation of the response.
 */
static VALUE
to_s(VALUE self) {
    agooResponse	res = (agooResponse)DATA_PTR(self);
    int			len = response_len(res);
    char		*s = ALLOC_N(char, len + 1);

    DEBUG_ALLOC(mem_to_s, s)
    response_fill(res, s);
    
    return rb_str_new(s, len);
}

/* Document-method: content
 *
 * call-seq: content()
 *
 * alias for _body_
 */

/* Document-method: body
 *
 * call-seq: body()
 *
 * Gets the HTTP body for the response.
 */
static VALUE
body_get(VALUE self) {
    agooResponse	res = (agooResponse)DATA_PTR(self);

    if (NULL == res->body) {
	return Qnil;
    }
    return rb_str_new(res->body, res->blen);
}

/* Document-method: content=
 *
 * call-seq: content=(str)
 *
 * alias for _body=_
 */

/* Document-method: body=
 *
 * call-seq: body=(str)
 *
 * Sets the HTTP body for the response.
 */
static VALUE
body_set(VALUE self, VALUE val) {
    agooResponse	res = (agooResponse)DATA_PTR(self);

    if (T_STRING == rb_type(val)) {
	res->body = strdup(StringValuePtr(val));
	DEBUG_ALLOC(mem_res_body, res->body)
	res->blen = (int)RSTRING_LEN(val);
    } else {
	rb_raise(rb_eArgError, "Expected a string");
	// TBD use Oj to encode val
    }
    return Qnil;
}

/* Document-method: code
 *
 * call-seq: code()
 *
 * Gets the HTTP status code for the response.
 */
static VALUE
code_get(VALUE self) {
    return INT2NUM(((agooResponse)DATA_PTR(self))->code);
}

/* Document-method: code=
 *
 * call-seq: code=(value)
 *
 * Sets the HTTP status code for the response.
 */
static VALUE
code_set(VALUE self, VALUE val) {
    int	code = NUM2INT(val);

    if (100 <= code && code < 600) {
	((agooResponse)DATA_PTR(self))->code = code;
    } else {
	rb_raise(rb_eArgError, "%d is not a valid HTTP status code.", code);
    }
    return Qnil;
}

/* Document-method: []
 *
 * call-seq: [](key)
 *
 * Gets a header element associated with the _key_.
 */
static VALUE
head_get(VALUE self, VALUE key) {
    agooResponse	res = (agooResponse)DATA_PTR(self);
    agooHeader		h;
    const char		*ks = StringValuePtr(key);
    int			klen = (int)RSTRING_LEN(key);
    
    for (h = res->headers; NULL != h; h = h->next) {
	if (0 == strncasecmp(h->text, ks, klen) && klen + 1 < h->len && ':' == h->text[klen]) {
	    return rb_str_new(h->text + klen + 2, h->len - klen - 4);
	}
    }
    return Qnil;
}

/* Document-method: []=
 *
 * call-seq: []=(key, value)
 *
 * Sets a header element with the _key_ and _value_ provided.
 */
static VALUE
head_set(VALUE self, VALUE key, VALUE val) {
    agooResponse	res = (agooResponse)DATA_PTR(self);
    agooHeader		h;
    agooHeader		prev = NULL;
    const char		*ks = StringValuePtr(key);
    const char		*vs;
    int			klen = (int)RSTRING_LEN(key);
    int			vlen;
    int			hlen;

    for (h = res->headers; NULL != h; h = h->next) {
	if (0 == strncasecmp(h->text, ks, klen) && klen + 1 < h->len && ':' == h->text[klen]) {
	    if (NULL == prev) {
		res->headers = h->next;
	    } else {
		prev->next = h->next;
	    }
	    DEBUG_FREE(mem_header, h);
	    xfree(h);
	    break;
	}
	prev = h;
    }
    if (T_STRING != rb_type(val)) {
	val = rb_funcall(val, rb_intern("to_s"), 0);
    }
    vs = StringValuePtr(val);
    vlen = (int)RSTRING_LEN(val);

    if (the_server.pedantic) {
	struct _agooErr	err = ERR_INIT;

	if (ERR_OK != http_header_ok(&err, ks, klen, vs, vlen)) {
	    rb_raise(rb_eArgError, "%s", err.msg);
	}
    }
    hlen = klen + vlen + 4;
    h = (agooHeader)ALLOC_N(char, sizeof(struct _agooHeader) - 8 + hlen + 1);
    DEBUG_ALLOC(mem_header, h)

    h->next = NULL;
    h->len = hlen;
    strncpy(h->text, ks, klen);
    strcpy(h->text + klen, ": ");
    strncpy(h->text + klen + 2, vs, vlen);
    strcpy(h->text + klen + 2 + vlen, "\r\n");
    if (NULL == res->headers) {
	res->headers = h;
    } else {
	for (prev = res->headers; NULL != prev->next; prev = prev->next) {
	}
	prev->next = h;
    }
    return Qnil;
}

agooText
response_text(VALUE self) {
    agooResponse	res = (agooResponse)DATA_PTR(self);
    int			len = response_len(res);
    agooText		t = text_allocate(len);

    response_fill(res, t->text);
    t->len = len;

    return t;
}

/* Document-class: Agoo::Response
 *
 * A response passed to a handler that responds to the _on_request_
 * method. The expected response is modified by the handler before returning.
 */
void
response_init(VALUE mod) {
    res_class = rb_define_class_under(mod, "Response", rb_cObject);

    rb_define_method(res_class, "to_s", to_s, 0);
    rb_define_method(res_class, "body", body_get, 0);
    rb_define_method(res_class, "body=", body_set, 1);
    rb_define_method(res_class, "content", body_get, 0);
    rb_define_method(res_class, "content=", body_set, 1);
    rb_define_method(res_class, "code", code_get, 0);
    rb_define_method(res_class, "code=", code_set, 1);
    rb_define_method(res_class, "[]", head_get, 1);
    rb_define_method(res_class, "[]=", head_set, 2);
}
