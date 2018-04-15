// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby/encoding.h>

#include "pub.h"
#include "server.h"
#include "upgraded.h"

static VALUE	upgraded_mod = Qundef;

static VALUE
up_write(VALUE self, VALUE msg) {
    uint64_t	cid = RB_FIX2ULONG(rb_ivar_get(self, rb_intern("@_cid")));
    Pub		p;

    // TBD check write status of cid
    
    printf("*** write called for %lu\n", cid);
    if (T_STRING == rb_type(msg)) {
	if (RB_ENCODING_IS_ASCII8BIT(msg)) {
	    p = pub_write(cid, StringValuePtr(msg), RSTRING_LEN(msg), true);
	} else {
	    p = pub_write(cid, StringValuePtr(msg), RSTRING_LEN(msg), false);
	}
    } else {
	volatile VALUE	rs = rb_funcall(msg, rb_intern("to_s"), 0);
	
	p = pub_write(cid, StringValuePtr(rs), RSTRING_LEN(rs), false);
    }
    printf("*** push to pub_queue\n");
    queue_push(&the_server->pub_queue, p);

    return Qnil;
}

static VALUE
up_subscribe(VALUE self, VALUE subject) {

    printf("*** subscribe called\n");

    // increment @_sid
    // create subscription
    // push pub onto server pub_queue

    // TBD create subscription object and return it
    return Qnil;
}

static VALUE
up_close(VALUE self) {

    printf("*** close called\n");

    return Qnil;
}

void
upgraded_extend(uint64_t cid, VALUE obj) {
    rb_ivar_set(obj, rb_intern("@_cid"), ULONG2NUM(cid));
    rb_ivar_set(obj, rb_intern("@_sid"), ULONG2NUM(0)); // used as counter for subscription id
    rb_extend_object(obj, upgraded_mod);
    rb_funcall(obj, rb_intern("on_open"), 0);
}

/* Document-module: Agoo::Upgraded
 *
 * Adds methods to a handler of WebSocket and SSe connections.
 */
void
upgraded_init(VALUE mod) {
    upgraded_mod = rb_define_module_under(mod, "Upgraded");

    rb_define_method(upgraded_mod, "write", up_write, 1);
    rb_define_method(upgraded_mod, "subscribe", up_subscribe, 1);
    rb_define_method(upgraded_mod, "close", up_close, 0);
    // TBD ready? or pending?
}
