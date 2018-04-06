// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include "server.h"
#include "upgraded.h"

static VALUE	upgraded_mod = Qundef;

static VALUE
up_write(VALUE self, VALUE msg) {

    printf("*** write called\n");

    return Qnil;
}

static VALUE
up_subscribe(VALUE self, VALUE subject) {

    printf("*** subscribe called\n");

    // TBD create sbscription object and return it
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
}
