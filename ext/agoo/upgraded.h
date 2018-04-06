// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_UPGRADED_H__
#define __AGOO_UPGRADED_H__

#include <stdint.h>

#include <ruby.h>

extern void	upgraded_init(VALUE mod);
extern void	upgraded_extend(uint64_t cid, VALUE obj);

#endif // __AGOO_UPGRADED_H__
