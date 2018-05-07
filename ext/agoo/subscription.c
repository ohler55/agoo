// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include "pub.h"
#include "queue.h"
#include "server.h"
#include "subscription.h"

static VALUE	subscription_class = Qundef;

VALUE
subscription_new(Upgraded up, uint64_t id, VALUE handler) {
    Subscription	s = ALLOC(struct _Subscription);

    s->up = up;
    s->id = id;
    s->handler = handler;
    s->self = Data_Wrap_Struct(subscription_class, NULL, xfree, s);

    return s->self;
}

/* Document-method: close
 *
 * call-seq: close()
 *
 * Close or unsubscribe a subscription.
 */
static VALUE
subscription_close(VALUE self) {
    Subscription	s = DATA_PTR(self);

    if (NULL != s->up && 0 != s->id) {
	Pub	p = pub_unsubscribe(s->up, s->id);
	
	queue_push(&the_server.pub_queue, (void*)p);
	queue_wakeup(&the_server.pub_queue);
	s->up = NULL;
	s->id = 0;
    }
    return Qnil;
}

/* Document-class: Agoo::Subscription
 *
 * The handle on a subscriptionscription used to register interest in messages
 * published on a subscriptionject. Published messages are delivered to Javascript
 * clients.
 */
void
subscription_init(VALUE mod) {
    subscription_class = rb_define_class_under(mod, "Subscription", rb_cObject);

    rb_define_method(subscription_class, "close", subscription_close, 0);
}
