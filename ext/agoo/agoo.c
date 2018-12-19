// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <signal.h>
#include <stdio.h>

#include <ruby.h>

#include "debug.h"
#include "error_stream.h"
#include "graphql.h"
#include "log.h"
#include "pub.h"
#include "rack_logger.h"
#include "request.h"
#include "rresponse.h"
#include "rlog.h"
#include "rserver.h"
#include "rupgraded.h"
#include "server.h"
#include "upgraded.h"

extern void	graphql_init(VALUE mod);

void
agoo_shutdown() {
    rserver_shutdown(Qnil);
    agoo_log_close();
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
    gql_destroy();
    debug_report();
    return Qnil;
}

/* Document-method: publish
 *
 * call-seq: publish(subject, message)
 *
 * Publish a message on the given subject. A subject is normally a String but
 * Symbols can also be used as can any other object that responds to #to_s.
 */
VALUE
ragoo_publish(VALUE self, VALUE subject, VALUE message) {
    int		slen;
    const char	*subj = extract_subject(subject, &slen);

    rb_check_type(message, T_STRING);
    agoo_server_publish(agoo_pub_publish(subj, slen, StringValuePtr(message), (int)RSTRING_LEN(message)));

    return Qnil;
}

/* Document-method: unsubscribe
 *
 * call-seq: unsubscribe(subject)
 *
 * Unsubscribes on client listeners on the specified subject. Subjects are
 * normally Strings but Symbols can also be used as can any other object that
 * responds to #to_s.
 */
static VALUE
ragoo_unsubscribe(VALUE self, VALUE subject) {
    rb_check_type(subject, T_STRING);

    agoo_server_publish(agoo_pub_unsubscribe(NULL, StringValuePtr(subject), (int)RSTRING_LEN(subject)));

    return Qnil;
}

static void
sig_handler(int sig) {
    agoo_shutdown();
    gql_destroy();
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

    rlog_init(mod);
    error_stream_init(mod);
    rack_logger_init(mod);
    request_init(mod);
    response_init(mod);
    server_init(mod);
    upgraded_init(mod);
    graphql_init(mod);

    rb_define_module_function(mod, "shutdown", ragoo_shutdown, 0);
    rb_define_module_function(mod, "publish", ragoo_publish, 2);
    rb_define_module_function(mod, "unsubscribe", ragoo_unsubscribe, 1);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);
}
