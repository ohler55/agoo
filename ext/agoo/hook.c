// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "con.h"
#include "debug.h"
#include "hook.h"
#include "req.h"

agooHook
agoo_hook_create(agooMethod method, const char *pattern, void *handler, agooHookType type, agooQueue q) {
    agooHook	hook = (agooHook)AGOO_MALLOC(sizeof(struct _agooHook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	if (NULL == pattern) {
	    if (AGOO_NONE != method) {
		if (NULL == (pat = AGOO_STRDUP(""))) {
		    AGOO_FREE(hook);
		    return NULL;
		}
	    }
	} else {
	    if (NULL == (pat = AGOO_STRDUP(pattern))) {
		AGOO_FREE(hook);
		return NULL;
	    }
	}
	hook->pattern = pat;
	hook->next = NULL;
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
    agooHook	hook = (agooHook)AGOO_MALLOC(sizeof(struct _agooHook));

    if (NULL != hook) {
	char	*pat = NULL;
	
	if (NULL == pattern) {
	    if (AGOO_NONE != method) {
		if (NULL == (pat = AGOO_STRDUP(""))) {
		    AGOO_FREE(hook);
		    return NULL;
		}
	    }
	} else {
	    if (NULL == (pat = AGOO_STRDUP(pattern))) {
		AGOO_FREE(hook);
		return NULL;
	    }
	}
	hook->pattern = pat;
	hook->next = NULL;
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
    AGOO_FREE(hook->pattern);
    AGOO_FREE(hook);
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
