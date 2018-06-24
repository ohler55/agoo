// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "hook.h"

Hook
hook_create(Method method, const char *pattern, void *handler, HookType type) {
    Hook	hook = (Hook)malloc(sizeof(struct _Hook));

    if (NULL != hook) {
	DEBUG_ALLOC(mem_hook, hook)
	if (NULL == pattern) {
	    pattern = "";
	}
	hook->next = NULL;
	hook->pattern = strdup(pattern);
	DEBUG_ALLOC(mem_hook_pattern, hook->pattern)
	hook->method = method;
	hook->handler = handler;
	hook->type = type;
    }
    return hook;
}

void
hook_destroy(Hook hook) {
    DEBUG_FREE(mem_hook_pattern, hook->pattern);
    DEBUG_FREE(mem_hook, hook)
    free(hook->pattern);
    free(hook);
}

bool
hook_match(Hook hook, Method method, const Seg path) {
    const char	*pat = hook->pattern;
    char	*p = path->start;
    char	*end = path->end;

    if (method != hook->method && ALL != hook->method) {
	return false;
    }
    for (; '\0' != *pat && p < end; pat++) {
	if (*p == *pat) {
	    p++;
	} else if ('*' == *pat) {
	    if ('*' == *(pat + 1)) {
		return true;
	    }
	    for (; p < end && '/' != *p; p++) {
	    }
	} else {
	    break;
	}
    }
    return '\0' == *pat && p == end;
}

Hook
hook_find(Hook hook, Method method, const Seg path) {
    for (; NULL != hook; hook = hook->next) {
	if (hook_match(hook, method, path)) {
	    return hook;
	}
    }
    return NULL;
}
