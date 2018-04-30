// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_ERROR_STREAM_H__
#define __AGOO_ERROR_STREAM_H__

#include <ruby.h>

#include "server.h"

extern void	error_stream_init(VALUE mod);
extern VALUE	error_stream_new();

#endif // __AGOO_ERROR_STREAM_H__
