// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "hook.h"
#include "req.h"

Hook
hook_create(Method method, const char *pattern, void *handler, HookType type, Queue q) {
    Hook	hook = (Hook)malloc(sizeof(struct _Hook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	DEBUG_ALLOC(mem_hook, hook);
	if (NULL == pattern) {
	    if (NONE != method) {
		pat = strdup("");
	    }
	} else {
	    pat = strdup(pattern);
	}
	hook->pattern = pat;

	hook->next = NULL;
	DEBUG_ALLOC(mem_hook_pattern, hook->pattern)
	hook->method = method;
	hook->handler = handler;
	hook->type = type;
	hook->queue = q;
	hook->no_queue = false;
    }
    return hook;
}

Hook
hook_func_create(Method method, const char *pattern, void (*func)(Req req), Queue q) {
    Hook	hook = (Hook)malloc(sizeof(struct _Hook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	DEBUG_ALLOC(mem_hook, hook);
	if (NULL == pattern) {
	    if (NONE != method) {
		pat = strdup("");
	    }
	} else {
	    pat = strdup(pattern);
	}
	hook->pattern = pat;

	hook->next = NULL;
	DEBUG_ALLOC(mem_hook_pattern, hook->pattern)
	hook->method = method;
	hook->func = func;
	hook->type = FUNC_HOOK;
	hook->queue = q;
	hook->no_queue = false;
    }
    return hook;
}

void
hook_destroy(Hook hook) {
    if (NULL != hook->pattern) {
	DEBUG_FREE(mem_hook_pattern, hook->pattern);
	free(hook->pattern);
    }
    DEBUG_FREE(mem_hook, hook)
    free(hook);
}

bool
hook_match(Hook hook, Method method, const Seg path) {
    const char	*pat = hook->pattern;
    char	*p = path->start;
    char	*end = path->end;

    if (1 < end - p && '/' == *(end - 1)) {
	end--;
    }
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
