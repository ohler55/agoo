// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RHOOK_H
#define AGOO_RHOOK_H

#include <ruby.h>

#include "hook.h"
#include "method.h"

extern Hook	rhook_create(Method method, const char *pattern, VALUE handler, Queue q);
extern VALUE	resolve_classpath(const char *name, size_t len);

#endif // AGOO_RHOOK_H
