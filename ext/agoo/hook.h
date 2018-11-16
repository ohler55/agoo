// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_HOOK_H
#define AGOO_HOOK_H

#include <stdbool.h>

#include "method.h"
#include "queue.h"
#include "seg.h"

struct _agooReq;

typedef enum {
    NO_HOOK		= '\0',
    RACK_HOOK		= 'R',
    BASE_HOOK		= 'B',
    WAB_HOOK		= 'W',
    PUSH_HOOK		= 'P',
    FUNC_HOOK		= 'F',
    FAST_HOOK		= 'O', // for OpO
} agooHookType;

typedef struct _agooHook {
    struct _agooHook	*next;
    agooMethod		method;
    char		*pattern;
    agooHookType	type;
    union {
	void		*handler;
	void		(*func)(struct _agooReq *req);
    };
    agooQueue		queue;
    bool		no_queue;
} *agooHook;

extern agooHook	hook_create(agooMethod method, const char *pattern, void *handler, agooHookType type, agooQueue q);
extern agooHook	hook_func_create(agooMethod method, const char *pattern, void (*func)(struct _agooReq *req), agooQueue q);
extern void	hook_destroy(agooHook hook);

extern bool	hook_match(agooHook hook, agooMethod method, const agooSeg seg);
extern agooHook	hook_find(agooHook hook, agooMethod method, const agooSeg seg);

#endif // AGOO_HOOK_H
