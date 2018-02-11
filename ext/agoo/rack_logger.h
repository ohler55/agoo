// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RACK_LOGGER_H__
#define __AGOO_RACK_LOGGER_H__

#include <ruby.h>

#include "server.h"

extern void	rack_logger_init(VALUE mod);
extern VALUE	rack_logger_new(Server server);

#endif // __AGOO_RACK_LOGGER_H__
