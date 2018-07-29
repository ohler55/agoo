// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RHOOK_H__
#define __AGOO_RHOOK_H__

#include <ruby.h>

#include "hook.h"
#include "method.h"

extern Hook	rhook_create(Method method, const char *pattern, VALUE handler, Queue q);

#endif // __AGOO_RHOOK_H__
