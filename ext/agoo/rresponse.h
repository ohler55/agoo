// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RRESPONSE_H__
#define __AGOO_RRESPONSE_H__

#include <ruby.h>

#include "text.h"

extern void	response_init(VALUE mod);
extern VALUE	response_new();
extern Text	response_text(VALUE self);

#endif // __AGOO_RRESPONSE_H__
