// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_ERROR_STREAM_H
#define AGOO_ERROR_STREAM_H

#include <ruby.h>

#include "server.h"

extern void	error_stream_init(VALUE mod);
extern VALUE	error_stream_new();

#endif // AGOO_ERROR_STREAM_H
