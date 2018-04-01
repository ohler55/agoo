// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "hook.h"

Hook
hook_create(Method method, const char *pattern, VALUE handler) {
    Hook	hook = (Hook)malloc(sizeof(struct _Hook));

    if (NULL != hook) {
	DEBUG_ALLOC(mem_hook)
	if (NULL == pattern) {
	    pattern = "";
	}
	hook->next = NULL;
	hook->handler = handler;
	rb_gc_register_address(&handler);
	hook->pattern = strdup(pattern);
	DEBUG_ALLOC(mem_hook_pattern)
	hook->method = method;
	if (rb_respond_to(handler, rb_intern("on_request"))) {
	    hook->type = BASE_HOOK;
	} else if (rb_respond_to(handler, rb_intern("call"))) {
	    hook->type = RACK_HOOK;
	} else if (rb_respond_to(handler, rb_intern("create")) &&
		   rb_respond_to(handler, rb_intern("read")) &&
		   rb_respond_to(handler, rb_intern("update")) &&
		   rb_respond_to(handler, rb_intern("delete"))) {
	    hook->type = WAB_HOOK;
	} else {
	    rb_raise(rb_eArgError, "handler does not have a on_request, or call method nor is it a WAB::Controller");
	}
    }
    return hook;
}

void
hook_destroy(Hook hook) {
    DEBUG_FREE(mem_hook_pattern)
    DEBUG_FREE(mem_hook)
    free(hook->pattern);
    free(hook);
}

bool
hook_match(Hook hook, Method method, const char *path, const char *pend) {
    const char	*pat = hook->pattern;

    if (method != hook->method && ALL != hook->method) {
	return false;
    }
    for (; '\0' != *pat && path < pend; pat++) {
	if (*path == *pat) {
	    path++;
	} else if ('*' == *pat) {
	    if ('*' == *(pat + 1)) {
		return true;
	    }
	    for (; path < pend && '/' != *path; path++) {
	    }
	} else {
	    break;
	}
    }
    return '\0' == *pat && path == pend;
}

Hook
hook_find(Hook hook, Method method, const char *path, const char *pend) {
    for (; NULL != hook; hook = hook->next) {
	if (hook_match(hook, method, path, pend)) {
	    return hook;
	}
    }
    return NULL;
}
