// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_HOOK_H__
#define __AGOO_HOOK_H__

#include <stdbool.h>

#include "method.h"
#include "queue.h"
#include "seg.h"

typedef enum {
    NO_HOOK		= '\0',
    RACK_HOOK		= 'R',
    BASE_HOOK		= 'B',
    WAB_HOOK		= 'W',
    PUSH_HOOK		= 'P',
    STDIO_HOOK		= 'S', // for OpO
    FAST_HOOK		= 'F', // for OpO
    GET_HOOK		= 'G', // for OpO
    TNEW_HOOK		= 'c', // for OpO
    TGET_HOOK		= 'r', // for OpO
    TUP_HOOK		= 'u', // for OpO
    TDEL_HOOK		= 'd', // for OpO
    TQL_HOOK		= 'T', // for OpO
    GRAPHQL_HOOK	= 'Q', // for OpO
} HookType;

typedef struct _Hook {
    struct _Hook	*next;
    Method		method;
    char		*pattern;
    HookType		type;
    void*		handler;
    Queue		queue;
} *Hook;

extern Hook	hook_create(Method method, const char *pattern, void *handler, HookType type, Queue q);
extern void	hook_destroy(Hook hook);

extern bool	hook_match(Hook hook, Method method, const Seg seg);
extern Hook	hook_find(Hook hook, Method method, const Seg seg);

#endif // __AGOO_HOOK_H__
