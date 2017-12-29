// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <ruby.h>

#include "error_stream.h"
#include "request.h"
#include "response.h"
#include "server.h"

void
Init_agoo() {
    VALUE	mod = rb_define_module("Agoo");

    error_stream_init(mod);
    request_init(mod);
    response_init(mod);
    server_init(mod);
}
