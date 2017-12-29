// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_HOOK_H__
#define __AGOO_HOOK_H__

#include <stdbool.h>

#include <ruby.h>

#include "types.h"

typedef enum {
    NO_HOOK	= '\0',
    RACK_HOOK	= 'R',
    BASE_HOOK	= 'B',
    WAB_HOOK	= 'W',
} HookType;

typedef struct _Hook {
    struct _Hook	*next;
    Method		method;
    VALUE		handler;
    char		*pattern;
    HookType		type;
} *Hook;

extern Hook	hook_create(Method method, const char *pattern, VALUE handler);
extern void	hook_destroy(Hook hook);

extern bool	hook_match(Hook hook, Method method, const char *path, const char *pend);
extern Hook	hook_find(Hook hook, Method method, const char *path, const char *pend);

#endif // __AGOO_HOOK_H__
