// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_REQUEST_H
#define AGOO_REQUEST_H

#include <stdint.h>

#include <ruby.h>

#include "req.h"

extern void	request_init(VALUE mod);
extern VALUE	request_wrap(Req req);
extern VALUE	request_env(Req req, VALUE self);

#endif // AGOO_REQUEST_H
