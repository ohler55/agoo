// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby/encoding.h>

#include "con.h"
#include "debug.h"
#include "pub.h"
#include "server.h"
#include "upgraded.h"

static VALUE	upgraded_class = Qundef;

static ID	on_open_id = 0;
static ID	to_s_id = 0;

static void
destroy(Upgraded up) {
    if (Qnil != up->wrap) {
	DATA_PTR(up->wrap) = NULL;
	up->wrap = Qnil;
    }
    if (NULL == up->prev) {
	the_server.up_list = up->next;
	if (NULL != up->next) {
	    up->next->prev = NULL;
	}
    } else {
	up->prev->next = up->next;
	if (NULL != up->next) {
	    up->next->prev = up->prev;
	}
    }
    DEBUG_FREE(mem_upgraded, up);
    free(up);
}

void
upgraded_release(Upgraded up) {
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	pthread_mutex_lock(&the_server.up_lock);
	destroy(up);
	pthread_mutex_unlock(&the_server.up_lock);
    }    
}

void
upgraded_release_con(Upgraded up) {
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	pthread_mutex_lock(&the_server.up_lock);
	up->con = NULL;
	destroy(up);
	pthread_mutex_unlock(&the_server.up_lock);
    }    
}

void
upgraded_ref(Upgraded up) {
    atomic_fetch_add(&up->ref_cnt, 1);
}

static Upgraded
get_upgraded(VALUE self) {
    Upgraded	up;

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    pthread_mutex_lock(&the_server.up_lock);
    if (NULL != (up = DATA_PTR(self))) {
	atomic_fetch_add(&up->ref_cnt, 1);
    }
    pthread_mutex_unlock(&the_server.up_lock);
    if (NULL == up) {
	rb_raise(rb_eIOError, "Connection closed.");
    }
    return up;
}

/* Document-method: write
 *
 * call-seq: write(msg)
 *
 * Writes a message to the WebSocket or SSe connection.
 */
static VALUE
up_write(VALUE self, VALUE msg) {
    Upgraded	up = get_upgraded(self);
    Pub		p;

    if (0 < the_server.max_push_pending) {
	int	pending = atomic_load(&up->pending);

	if (the_server.max_push_pending <= pending) {
	    atomic_fetch_sub(&up->ref_cnt, 1);
	    rb_raise(rb_eIOError, "Too many writes pending. Try again later.");
	}
    }
    if (T_STRING == rb_type(msg)) {
	if (RB_ENCODING_IS_ASCII8BIT(msg)) {
	    p = pub_write(up, StringValuePtr(msg), RSTRING_LEN(msg), true);
	} else {
	    p = pub_write(up, StringValuePtr(msg), RSTRING_LEN(msg), false);
	}
    } else {
	volatile VALUE	rs = rb_funcall(msg, to_s_id, 0);
	
	p = pub_write(up, StringValuePtr(rs), RSTRING_LEN(rs), false);
    }
    atomic_fetch_add(&up->pending, 1);
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

    // increment _sid
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
    Upgraded	up = get_upgraded(self);

    atomic_fetch_add(&up->pending, 1);
    queue_push(&the_server.pub_queue, pub_close(up));

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
    Upgraded	up = get_upgraded(self);
    int		pending = atomic_load(&up->pending);

    atomic_fetch_sub(&up->ref_cnt, 1);

    return INT2NUM(pending);
}

Upgraded
upgraded_create(Con c, VALUE obj) {
    Upgraded	up = (Upgraded)malloc(sizeof(struct _Upgraded));

    if (!the_server.active) {
	rb_raise(rb_eIOError, "Server shutdown.");
    }
    if (NULL != up) {
	DEBUG_ALLOC(mem_upgraded, up);
	up->con = c;
	up->handler = obj;
	atomic_init(&up->pending, 0);
	atomic_init(&up->ref_cnt, 1); // start with 1 for the Con reference
	up->on_empty = rb_respond_to(obj, rb_intern("on_drained"));
	up->on_close = rb_respond_to(obj, rb_intern("on_close"));
	up->on_shut = rb_respond_to(obj, rb_intern("on_shutdown"));
	up->on_msg = rb_respond_to(obj, rb_intern("on_message"));
	up->wrap = Data_Wrap_Struct(upgraded_class, NULL, NULL, up);
	up->prev = NULL;
	pthread_mutex_lock(&the_server.up_lock);
	if (NULL == the_server.up_list) {
	    up->next = NULL;
	} else {
	    the_server.up_list->prev = up;
	}
	up->next = the_server.up_list;
	the_server.up_list = up;
	c->up = up;
	pthread_mutex_unlock(&the_server.up_lock);

	if (rb_respond_to(obj, on_open_id)) {
	    rb_funcall(obj, on_open_id, 1, up->wrap);
	}
    }
    return up;
}

/* Document-module: Agoo::Upgraded
 *
 * Adds methods to a handler of WebSocket and SSe connections.
 */
void
upgraded_init(VALUE mod) {
    upgraded_class = rb_define_class_under(mod, "Upgraded", rb_cObject);

    rb_define_method(upgraded_class, "write", up_write, 1);
    rb_define_method(upgraded_class, "subscribe", up_subscribe, 1);
    rb_define_method(upgraded_class, "close", up_close, 0);
    rb_define_method(upgraded_class, "pending", pending, 0);

    on_open_id = rb_intern("on_open");
    to_s_id = rb_intern("to_s");
    
}
