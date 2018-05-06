// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_UPGRADED_H__
#define __AGOO_UPGRADED_H__

#include <stdint.h>

#include <ruby.h>

struct _Con;

typedef struct _Upgraded {
    struct _Upgraded	*next;
    struct _Upgraded	*prev;
    struct _Con		*con;
    VALUE		handler;
    VALUE		wrap;
    atomic_int		pending;
    atomic_int		ref_cnt;
    bool		on_empty;
    bool		on_close;
    bool		on_shut;
    bool		on_msg;
} *Upgraded;

extern void	upgraded_init(VALUE mod);
extern Upgraded	upgraded_create(struct _Con *c, VALUE obj);
extern void	upgrade_release(Upgraded up);
extern void	upgrade_release_con(Upgraded up);

#endif // __AGOO_UPGRADED_H__
