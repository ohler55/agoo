// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RRESPONSE_H
#define AGOO_RRESPONSE_H

#include <ruby.h>

#include "text.h"

extern void	response_init(VALUE mod);
extern VALUE	response_new();
extern Text	response_text(VALUE self);

#endif // AGOO_RRESPONSE_H
