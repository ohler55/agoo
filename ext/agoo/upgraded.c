// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby/encoding.h>

#include "con.h"
#include "debug.h"
#include "pub.h"
#include "server.h"
#include "subject.h"
#include "upgraded.h"

static VALUE	upgraded_class = Qundef;

static VALUE	sse_sym;
static VALUE	websocket_sym;

static ID	on_open_id = 0;
static ID	to_s_id = 0;

static void
destroy(Upgraded up) {
    Subject	subject;

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
    while (NULL != (subject = up->subjects)) {
	up->subjects = up->subjects->next;
	subject_destroy(subject);
    }
    DEBUG_FREE(mem_upgraded, up);
    free(up);
}

void
upgraded_release(Upgraded up) {
    pthread_mutex_lock(&the_server.up_lock);
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	destroy(up);
    }    
    pthread_mutex_unlock(&the_server.up_lock);
}

void
upgraded_release_con(Upgraded up) {
    pthread_mutex_lock(&the_server.up_lock);
    up->con = NULL;
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	destroy(up);
    }
    pthread_mutex_unlock(&the_server.up_lock);
}

// Called from the con_loop thread, no need to lock, this steals the subject
// so the pub subject should set to NULL
void
upgraded_add_subject(Upgraded up, Subject subject) {
    Subject	s;

    for (s = up->subjects; NULL != s; s = s->next) {
	if (0 == strcmp(subject->pattern, s->pattern)) {
	    subject_destroy(subject);
	    return;
	}
    }
    subject->next = up->subjects;
    up->subjects = subject;
}

void
upgraded_del_subject(Upgraded up, Subject subject) {
    if (NULL == subject) {
	while (NULL != (subject = up->subjects)) {
	    up->subjects = up->subjects->next;
	    subject_destroy(subject);
	}
    } else {
	Subject	s;
	Subject	prev = NULL;

	for (s = up->subjects; NULL != s; s = s->next) {
	    if (0 == strcmp(subject->pattern, s->pattern)) {
		if (NULL == prev) {
		    up->subjects = s->next;
		} else {
		    prev->next = s->next;
		}
		subject_destroy(s);
		break;
	    }
	    prev = s;
	}
    }
}

bool
upgraded_match(Upgraded up, const char *subject) {
    Subject	s;

    for (s = up->subjects; NULL != s; s = s->next) {
	if (subject_check(s, subject)) {
	    return true;
	}
    }
    return false;
}

void
upgraded_ref(Upgraded up) {
    atomic_fetch_add(&up->ref_cnt, 1);
}

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

/* Document-method: write
 *
 * call-seq: write(msg)
 *
 * Writes a message to the WebSocket or SSE connection. Returns true if the
 * message has been queued and false otherwise. A closed connection or too
 * many pending messages could cause a value of false to be returned.
 */
static VALUE
up_write(VALUE self, VALUE msg) {
    Upgraded	up = get_upgraded(self);
    Pub		p;

    if (NULL == up) {
	return Qfalse;
    }
    if (0 < the_server.max_push_pending && the_server.max_push_pending <= atomic_load(&up->pending)) {
	atomic_fetch_sub(&up->ref_cnt, 1);
	// Too many pending messages.
	return Qfalse;
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

    return Qtrue;
}

/* Document-method: subscribe
 *
 * call-seq: subscribe(subject)
 *
 * Subscribes to messages published on the specified subject. The subject is a
 * dot delimited string that can include a '*' character as a wild card that
 * matches any set of characters. The '>' character matches all remaining
 * characters. Examples: people.fred.log, people.*.log, people.fred.>
 */
static VALUE
up_subscribe(VALUE self, VALUE subject) {
    Upgraded	up;
    
    rb_check_type(subject, T_STRING);
    if (NULL != (up = get_upgraded(self))) {
	atomic_fetch_add(&up->pending, 1);
	queue_push(&the_server.pub_queue, pub_subscribe(up, StringValuePtr(subject), RSTRING_LEN(subject)));
    }
    return Qnil;
}

/* Document-method: unsubscribe
 *
 * call-seq: unsubscribe(subject=nil)
 *
 * Unsubscribes to messages on the provided subject. If the subject is nil
 * then all subscriptions for the object are removed.
 */
static VALUE
up_unsubscribe(int argc, VALUE *argv, VALUE self) {
    Upgraded	up;
    const char	*subject = NULL;
    int		slen = 0;

    if (0 < argc) {
	rb_check_type(argv[0], T_STRING);
	subject = StringValuePtr(argv[0]);
	slen = (int)RSTRING_LEN(argv[0]);
    }
    if (NULL != (up = get_upgraded(self))) {
	atomic_fetch_add(&up->pending, 1);
	queue_push(&the_server.pub_queue, pub_unsubscribe(up, subject, slen));
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
up_close(VALUE self) {
    Upgraded	up = get_upgraded(self);

    if (NULL != up) {
	atomic_fetch_add(&up->pending, 1);
	queue_push(&the_server.pub_queue, pub_close(up));
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
pending(VALUE self) {
    Upgraded	up = get_upgraded(self);
    int		pending = -1;
    
    if (NULL != up) {
	pending = atomic_load(&up->pending);
	atomic_fetch_sub(&up->ref_cnt, 1);
    }
    return INT2NUM(pending);
}

/* Document-method: protocol
 *
 * call-seq: protocol()
 *
 * Returns the protocol of the upgraded connection as either :websocket or
 * :sse. If not longer connected nil is returned.
 */
static VALUE
protocol(VALUE self) {
    VALUE	pro = Qnil;

    if (the_server.active) {
	Upgraded	up;
	
	pthread_mutex_lock(&the_server.up_lock);
	if (NULL != (up = DATA_PTR(self)) && NULL != up->con) {
	    switch (up->con->kind) {
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
	up->subjects = NULL;
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

// Use the publish from the Agoo module.
extern VALUE	ragoo_publish(VALUE self, VALUE subject, VALUE message);

/* Document-module: Agoo::Upgraded
 *
 * Adds methods to a handler of WebSocket and SSE connections.
 */
void
upgraded_init(VALUE mod) {
    upgraded_class = rb_define_class_under(mod, "Upgraded", rb_cObject);

    rb_define_method(upgraded_class, "write", up_write, 1);
    rb_define_method(upgraded_class, "subscribe", up_subscribe, 1);
    rb_define_method(upgraded_class, "unsubscribe", up_unsubscribe, -1);
    rb_define_method(upgraded_class, "close", up_close, 0);
    rb_define_method(upgraded_class, "pending", pending, 0);
    rb_define_method(upgraded_class, "protocol", protocol, 0);
    rb_define_method(upgraded_class, "publish", ragoo_publish, 2);

    on_open_id = rb_intern("on_open");
    to_s_id = rb_intern("to_s");

    sse_sym = ID2SYM(rb_intern("sse"));				rb_gc_register_address(&sse_sym);
    websocket_sym = ID2SYM(rb_intern("websocket"));		rb_gc_register_address(&websocket_sym);
}
