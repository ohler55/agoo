// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "hook.h"

static VALUE
resolve_classname(VALUE mod, const char *classname) {
    VALUE	clas;
    ID		ci = rb_intern(classname);

    if (rb_const_defined_at(mod, ci)) {
	clas = rb_const_get_at(mod, ci);
    } else {
	clas = Qundef;
    }
    return clas;
}

static VALUE
resolve_classpath(const char *name, size_t len) {
    char	class_name[1024];
    VALUE	clas;
    char	*end = class_name + sizeof(class_name) - 1;
    char	*s;
    const char	*n = name;

    clas = rb_cObject;
    for (s = class_name; 0 < len; n++, len--) {
	if (':' == *n) {
	    *s = '\0';
	    n++;
	    len--;
	    if (':' != *n) {
		return Qundef;
	    }
	    if (Qundef == (clas = resolve_classname(clas, class_name))) {
		return Qundef;
	    }
	    s = class_name;
	} else if (end <= s) {
	    return Qundef;
	} else {
	    *s++ = *n;
	}
    }
    *s = '\0';
    return resolve_classname(clas, class_name);
}

Hook
hook_create(Method method, const char *pattern, VALUE handler) {
    Hook	hook = (Hook)malloc(sizeof(struct _Hook));

    if (NULL != hook) {
	DEBUG_ALLOC(mem_hook, hook)
	if (NULL == pattern) {
	    pattern = "";
	}
	hook->next = NULL;
	if (T_STRING == rb_type(handler)) {
	    handler = resolve_classpath(StringValuePtr(handler), RSTRING_LEN(handler));
	}
	hook->handler = handler;
	rb_gc_register_address(&handler);
	hook->pattern = strdup(pattern);
	DEBUG_ALLOC(mem_hook_pattern, hook->pattern)
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
    DEBUG_FREE(mem_hook_pattern, hook->pattern)
	DEBUG_FREE(mem_hook, hook)
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
