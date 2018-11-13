// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RACK_LOGGER_H
#define AGOO_RACK_LOGGER_H

#include <ruby.h>

#include "server.h"

extern void	rack_logger_init(VALUE mod);
extern VALUE	rack_logger_new();

#endif // AGOO_RACK_LOGGER_H
