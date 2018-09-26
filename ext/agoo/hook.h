// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_HOOK_H__
#define __AGOO_HOOK_H__

#include <stdbool.h>

#include "method.h"
#include "queue.h"
#include "seg.h"

struct _Req;

typedef enum {
    NO_HOOK		= '\0',
    RACK_HOOK		= 'R',
    BASE_HOOK		= 'B',
    WAB_HOOK		= 'W',
    PUSH_HOOK		= 'P',
    FUNC_HOOK		= 'F',
    FAST_HOOK		= 'O', // for OpO
} HookType;

typedef struct _Hook {
    struct _Hook	*next;
    Method		method;
    char		*pattern;
    HookType		type;
    union {
	void		*handler;
	void		(*func)(struct _Req *req);
    };
    Queue		queue;
} *Hook;

extern Hook	hook_create(Method method, const char *pattern, void *handler, HookType type, Queue q);
extern Hook	hook_func_create(Method method, const char *pattern, void (*func)(struct _Req *req), Queue q);
extern void	hook_destroy(Hook hook);

extern bool	hook_match(Hook hook, Method method, const Seg seg);
extern Hook	hook_find(Hook hook, Method method, const Seg seg);

#endif // __AGOO_HOOK_H__
