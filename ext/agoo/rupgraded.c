// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby.h>
#include <ruby/encoding.h>

#include "con.h"
#include "debug.h"
#include "pub.h"
#include "rserver.h"
#include "rupgraded.h"
#include "server.h"
#include "subject.h"
#include "upgraded.h"

static VALUE	upgraded_class = Qundef;

static VALUE	sse_sym;
static VALUE	websocket_sym;

static ID	on_open_id = 0;
static ID	to_s_id = 0;

static Upgraded
get_upgraded(VALUE self) {
    Upgraded	up = NULL;

    if (the_server.active) {
	pthread_mutex_lock(&the_server.up_lock);
	if (NULL != (up = DATA_PTR(self))) {
	    atomic_fetch_add(&up->ref_cnt, 1);
	}
	pthread_mutex_unlock(&the_server.up_lock);
    }
    return up;
}

const char*
extract_subject(VALUE subject, int *slen) {
    const char	*subj;
    
    switch (rb_type(subject)) {
    case T_STRING:
	subj = StringValuePtr(subject);
	*slen = (int)RSTRING_LEN(subject);
	break;
    case T_SYMBOL:
	subj = rb_id2name(rb_sym2id(subject));
	*slen = strlen(subj);
	break;
    default:
	subject = rb_funcall(subject, to_s_id, 0);
	subj = StringValuePtr(subject);
	*slen = (int)RSTRING_LEN(subject);
	break;
    }
    return subj;
}

/* Document-method: write
 *
 * call-seq: write(msg)
 *
 * Writes a message to the WebSocket or SSE connection. Returns true if the
 * message has been queued and false otherwise. A closed connection or too
 * many pending messages could cause a value of false to be returned.
 */
static VALUE
rup_write(VALUE self, VALUE msg) {
    Upgraded	up = get_upgraded(self);
    const char	*message;
    size_t	mlen;
    bool	bin = false;

    if (NULL == up) {
	return Qfalse;
    }
    if (T_STRING == rb_type(msg)) {
	message = StringValuePtr(msg);
	mlen = RSTRING_LEN(msg);
	if (RB_ENCODING_IS_ASCII8BIT(msg)) {
	    bin = true;
	}
    } else {
	volatile VALUE	rs = rb_funcall(msg, to_s_id, 0);
	
	message = StringValuePtr(rs);
	mlen = RSTRING_LEN(rs);
    }
    return upgraded_write(up, message, mlen, bin, false) ? Qtrue : Qfalse;
}

/* Document-method: subscribe
 *
 * call-seq: subscribe(subject)
 *
 * Subscribes to messages published on the specified subject. The subject is a
 * dot delimited string that can include a '*' character as a wild card that
 * matches any set of characters. The '>' character matches all remaining
 * characters. Examples: people.fred.log, people.*.log, people.fred.>
 *
 * Symbols can also be used as can any other object that responds to #to_s.
 */
static VALUE
rup_subscribe(VALUE self, VALUE subject) {
    Upgraded	up;
    int		slen;
    const char	*subj = extract_subject(subject, &slen);

    if (NULL != (up = get_upgraded(self))) {
	upgraded_subscribe(up, subj, slen, false);
    }
    return Qnil;
}

/* Document-method: unsubscribe
 *
 * call-seq: unsubscribe(subject=nil)
 *
 * Unsubscribes to messages on the provided subject. If the subject is nil
 * then all subscriptions for the object are removed.
 *
 * Symbols can also be used as can any other object that responds to #to_s.
 */
static VALUE
rup_unsubscribe(int argc, VALUE *argv, VALUE self) {
    Upgraded	up;
    const char	*subject = NULL;
    int		slen = 0;

    if (0 < argc) {
	subject = extract_subject(argv[0], &slen);
    }
    if (NULL != (up = get_upgraded(self))) {
	upgraded_unsubscribe(up, subject, slen, false);
    }
    return Qnil;
}

/* Document-method: close
 *
 * call-seq: close()
 *
 * Closes the connections associated with the handler.
 */
static VALUE
rup_close(VALUE self) {
    Upgraded	up = get_upgraded(self);

    if (NULL != up) {
	upgraded_close(up, false);
    }
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
rup_pending(VALUE self) {
    Upgraded	up = get_upgraded(self);
    int		pending = -1;
    
    if (NULL != up) {
	pending = upgraded_pending(up);
	atomic_fetch_sub(&up->ref_cnt, 1);
    }
    return INT2NUM(pending);
}

/* Document-method: open?
 *
 * call-seq: open?()
 *
 * Returns true if the connection is open and false otherwise.
 */
static VALUE
rup_open(VALUE self) {
    Upgraded	up = get_upgraded(self);
    int		pending = -1;
    
    if (NULL != up) {
	pending = atomic_load(&up->pending);
	atomic_fetch_sub(&up->ref_cnt, 1);
    }
    return 0 <= pending ? Qtrue : Qfalse;
}

/* Document-method: protocol
 *
 * call-seq: protocol()
 *
 * Returns the protocol of the upgraded connection as either :websocket or
 * :sse. If no longer connected nil is returned.
 */
static VALUE
rup_protocol(VALUE self) {
    VALUE	pro = Qnil;

    if (the_server.active) {
	Upgraded	up;
	
	pthread_mutex_lock(&the_server.up_lock);
	if (NULL != (up = DATA_PTR(self)) && NULL != up->con) {
	    switch (up->con->bind->kind) {
	    case CON_WS:
		pro = websocket_sym;
		break;
	    case CON_SSE:
		pro = sse_sym;
		break;
	    default:
		break;
	    }
	}
	pthread_mutex_unlock(&the_server.up_lock);
    }
    return pro;
}

static void
on_destroy(Upgraded up) {
    if (Qnil != (VALUE)up->wrap) {
	DATA_PTR((VALUE)up->wrap) = NULL;
	up->wrap = (void*)Qnil;
    }
}

Upgraded
rupgraded_create(Con c, VALUE obj, VALUE env) {
    Upgraded	up;

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    if (NULL != (up = upgraded_create(c, (void*)obj, (void*)env))) {
	up->on_empty = rb_respond_to(obj, rb_intern("on_drained"));
	up->on_close = rb_respond_to(obj, rb_intern("on_close"));
	up->on_shut = rb_respond_to(obj, rb_intern("on_shutdown"));
	up->on_msg = rb_respond_to(obj, rb_intern("on_message"));
	up->on_error = rb_respond_to(obj, rb_intern("on_error"));
	up->on_destroy = on_destroy;

	up->wrap = (void*)Data_Wrap_Struct(upgraded_class, NULL, NULL, up);

	server_add_upgraded(up);
	
	if (rb_respond_to(obj, on_open_id)) {
	    rb_funcall(obj, on_open_id, 1, (VALUE)up->wrap);
	}
    }
    return up;
}

// Use the publish from the Agoo module.
extern VALUE	ragoo_publish(VALUE self, VALUE subject, VALUE message);

/* Document-method: env
 *
 * call-seq: env()
 *
 * Returns the environment passed to the call method that initiated the
 * Upgraded Object creation.
 */
static VALUE
env(VALUE self) {
    Upgraded	up = get_upgraded(self);

    if (NULL != up) {
	return (VALUE)up->env;
    }
    return Qnil;
}

/* Document-module: Agoo::Upgraded
 *
 * Adds methods to a handler of WebSocket and SSE connections.
 */
void
upgraded_init(VALUE mod) {
    upgraded_class = rb_define_class_under(mod, "Upgraded", rb_cObject);

    rb_define_method(upgraded_class, "write", rup_write, 1);
    rb_define_method(upgraded_class, "subscribe", rup_subscribe, 1);
    rb_define_method(upgraded_class, "unsubscribe", rup_unsubscribe, -1);
    rb_define_method(upgraded_class, "close", rup_close, 0);
    rb_define_method(upgraded_class, "pending", rup_pending, 0);
    rb_define_method(upgraded_class, "protocol", rup_protocol, 0);
    rb_define_method(upgraded_class, "publish", ragoo_publish, 2);
    rb_define_method(upgraded_class, "open?", rup_open, 0);
    rb_define_method(upgraded_class, "env", env, 0);

    on_open_id = rb_intern("on_open");
    to_s_id = rb_intern("to_s");

    sse_sym = ID2SYM(rb_intern("sse"));				rb_gc_register_address(&sse_sym);
    websocket_sym = ID2SYM(rb_intern("websocket"));		rb_gc_register_address(&websocket_sym);
}
