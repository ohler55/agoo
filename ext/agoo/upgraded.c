// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby/encoding.h>

#include "pub.h"
#include "server.h"
#include "upgraded.h"

static VALUE	upgraded_mod = Qundef;

static ID	cid_id = 0;
static ID	sid_id = 0;
static ID	on_open_id = 0;
static ID	to_s_id = 0;

/* Document-method: write
 *
 * call-seq: write(msg)
 *
 * Writes a message to the WebSocket or SSe connection.
 */
static VALUE
up_write(VALUE self, VALUE msg) {
    uint64_t	cid = RB_FIX2ULONG(rb_ivar_get(self, cid_id));
    Pub		p;

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    if (0 < the_server.max_push_pending) {
	int	pending = cc_get_pending(&the_server.con_cache, cid);

	if (the_server.max_push_pending <= pending) {
	    rb_raise(rb_eIOError, "Too many writes pending. Try again later.");
	}
    }
    if (T_STRING == rb_type(msg)) {
	if (RB_ENCODING_IS_ASCII8BIT(msg)) {
	    p = pub_write(cid, StringValuePtr(msg), RSTRING_LEN(msg), true);
	} else {
	    p = pub_write(cid, StringValuePtr(msg), RSTRING_LEN(msg), false);
	}
    } else {
	volatile VALUE	rs = rb_funcall(msg, to_s_id, 0);
	
	p = pub_write(cid, StringValuePtr(rs), RSTRING_LEN(rs), false);
    }
    cc_pending_inc(&the_server.con_cache, cid);
    queue_push(&the_server.pub_queue, p);

    return Qnil;
}

/* Document-method: subscribe
 *
 * call-seq: subscribe(subject)
 *
 * Subscribes to messages published on the specified subject. (not implemented
 * yet)
 */
static VALUE
up_subscribe(VALUE self, VALUE subject) {

    // printf("*** subscribe called\n");

    // increment @_sid
    // create subscription
    // push pub onto server pub_queue
    // TBD create subscription object and return it

    return Qnil;
}

/* Document-method: close
 *
 * call-seq: close()
 *
 * Closes the connections associated with the handler.
 */
static VALUE
up_close(VALUE self) {
    uint64_t	cid = RB_FIX2ULONG(rb_ivar_get(self, cid_id));

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    queue_push(&the_server.pub_queue, pub_close(cid));

    return Qnil;
}

/* Document-method: pending
 *
 * call-seq: pending()
 *
 * Returns the number of pending WebSocket or SSE writes. If the connection is
 * closed then -1 is returned.
 */
static VALUE
pending(VALUE self) {
    uint64_t	cid = RB_FIX2ULONG(rb_ivar_get(self, cid_id));

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    return INT2NUM(cc_get_pending(&the_server.con_cache, cid));
}

void
upgraded_extend(uint64_t cid, VALUE obj) {
    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    rb_ivar_set(obj, cid_id, ULONG2NUM(cid));
    rb_ivar_set(obj, sid_id, ULONG2NUM(0)); // used as counter for subscription id
    rb_extend_object(obj, upgraded_mod);
    if (rb_respond_to(obj, on_open_id)) {
	rb_funcall(obj, on_open_id, 0);
    }
    cc_set_handler(&the_server.con_cache, cid, obj,
		   rb_respond_to(obj, rb_intern("on_drained")),
		   rb_respond_to(obj, rb_intern("on_close")),
		   rb_respond_to(obj, rb_intern("on_shutdown")),
		   rb_respond_to(obj, rb_intern("on_message")));
    rb_funcall(obj, rb_intern("to_s"), 0);
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
    rb_define_method(upgraded_mod, "pending", pending, 0);

    cid_id = rb_intern("@_cid");
    sid_id = rb_intern("@_sid");
    on_open_id = rb_intern("on_open");
    to_s_id = rb_intern("to_s");
    
}
