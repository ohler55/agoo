// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <ruby.h>

#include "error_stream.h"
#include "rack_logger.h"
#include "request.h"
#include "response.h"
#include "server.h"
#include "subscription.h"
#include "upgraded.h"

/* Document-module: Agoo
 *
 * A High Performance HTTP Server that supports the Ruby rack API. The word
 * agoo is a Japanese word for a type of flying fish.
 */
void
Init_agoo() {
    VALUE	mod = rb_define_module("Agoo");

    error_stream_init(mod);
    rack_logger_init(mod);
    request_init(mod);
    response_init(mod);
    server_init(mod);
    subscription_init(mod);
    upgraded_init(mod);
}
