// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "con.h"
#include "debug.h"
#include "hook.h"
#include "req.h"

agooHook
agoo_hook_create(agooMethod method, const char *pattern, void *handler, agooHookType type, agooQueue q) {
    agooHook	hook = (agooHook)malloc(sizeof(struct _agooHook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	DEBUG_ALLOC(mem_hook, hook);
	if (NULL == pattern) {
	    if (AGOO_NONE != method) {
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

agooHook
agoo_hook_func_create(agooMethod method, const char *pattern, void (*func)(agooReq req), agooQueue q) {
    agooHook	hook = (agooHook)malloc(sizeof(struct _agooHook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	DEBUG_ALLOC(mem_hook, hook);
	if (NULL == pattern) {
	    if (AGOO_NONE != method) {
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
agoo_hook_destroy(agooHook hook) {
    if (NULL != hook->pattern) {
	DEBUG_FREE(mem_hook_pattern, hook->pattern);
	free(hook->pattern);
    }
    DEBUG_FREE(mem_hook, hook)
    free(hook);
}

bool
agoo_hook_match(agooHook hook, agooMethod method, const agooSeg path) {
    const char	*pat = hook->pattern;
    char	*p = path->start;
    char	*end = path->end;

    if (1 < end - p && '/' == *(end - 1)) {
	end--;
    }
    if (method != hook->method && AGOO_ALL != hook->method) {
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

agooHook
agoo_hook_find(agooHook hook, agooMethod method, const agooSeg path) {
    for (; NULL != hook; hook = hook->next) {
	if (agoo_hook_match(hook, method, path)) {
	    return hook;
	}
    }
    return NULL;
}
