// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <signal.h>
#include <stdio.h>

#include <ruby.h>

#include "debug.h"
#include "error_stream.h"
#include "log.h"
#include "rack_logger.h"
#include "request.h"
#include "response.h"
#include "server.h"
#include "subscription.h"
#include "upgraded.h"

void
agoo_shutdown() {
    server_shutdown();
    log_close();
}

/* Document-method: shutdown
 *
 * call-seq: shutdown()
 *
 * Shutdown the server and logger.
 */
static VALUE
ragoo_shutdown(VALUE self) {
    agoo_shutdown();
    debug_rreport();
    return Qnil;
}

static void
sig_handler(int sig) {
    agoo_shutdown();

    debug_report();
    // Use exit instead of rb_exit as rb_exit segfaults most of the time.
    //rb_exit(0);
    exit(0);
}


/* Document-module: Agoo
 *
 * A High Performance HTTP Server that supports the Ruby rack API. The word
 * agoo is a Japanese word for a type of flying fish.
 */
void
Init_agoo() {
    VALUE	mod = rb_define_module("Agoo");

    log_init(mod);
    error_stream_init(mod);
    rack_logger_init(mod);
    request_init(mod);
    response_init(mod);
    server_init(mod);
    subscription_init(mod);
    upgraded_init(mod);

    rb_define_module_function(mod, "shutdown", ragoo_shutdown, 0);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);
}
