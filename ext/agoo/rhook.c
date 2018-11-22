// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include <ruby.h>

#include "debug.h"
#include "rhook.h"
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

VALUE
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

agooHook
rhook_create(agooMethod method, const char *pattern, VALUE handler, agooQueue q) {
    agooHook	hook = agoo_hook_create(method, pattern, NULL, RACK_HOOK, q);

    if (NULL != hook) {
	if (T_STRING == rb_type(handler)) {
	    handler = resolve_classpath(StringValuePtr(handler), RSTRING_LEN(handler));
	    // TBD does class handle it or should an instance be made?
	    //  
	}
	hook->handler = (void*)handler;
	rb_gc_register_address(&handler);
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
