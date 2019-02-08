// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "rack_logger.h"
#include "debug.h"
#include "log.h"
#include "text.h"

static VALUE	rl_class = Qundef;

VALUE
rack_logger_new() {
    return rb_class_new_instance(0, NULL, rl_class);
}

static void
log_message(agooLogCat cat, VALUE message) {
    volatile VALUE	rs = rb_funcall(message, rb_intern("to_s"), 0);

    rb_check_type(rs, T_STRING);
    if (!agoo_server.active) {
	return;
    }
    if (rb_block_given_p()) {
	agooText	text = agoo_text_create(StringValuePtr(rs), (int)RSTRING_LEN(rs));
	volatile VALUE	x = rb_yield_values(0);

	if (Qnil != x) {
	    rs = rb_funcall(x, rb_intern("to_s"), 0);
	    text = agoo_text_append(text, ": ", 2);
	    text = agoo_text_append(text, StringValuePtr(rs), (int)RSTRING_LEN(rs));
	}
	if (NULL == text) {
	    rb_raise(rb_eNoMemError, "Failed to allocate memory for a log message.");
	}
	agoo_log_cat(cat, "%s", text->text);
	agoo_text_release(text);
    } else {
	agoo_log_cat(cat, "%s", StringValuePtr(rs));
    }
}

/* Document-method: debug
 *
 * call-seq: debug(message, &block)
 *
 * Calls the Server#debug method.
 */
static VALUE
rl_debug(VALUE self, VALUE message) {
    log_message(&agoo_debug_cat, message);
    return Qnil;
}

/* Document-method: info
 *
 * call-seq: info(message, &block)
 *
 * Calls the Server#info method.
 */
static VALUE
rl_info(VALUE self, VALUE message) {
    log_message(&agoo_info_cat, message);
    return Qnil;
}

/* Document-method: warn
 *
 * call-seq: warn(message, &block)
 *
 * Calls the Server#warn method.
 */
static VALUE
rl_warn(VALUE self, VALUE message) {
    log_message(&agoo_warn_cat, message);
    return Qnil;
}

/* Document-method: error
 *
 * call-seq: error(message, &block)
 *
 * Calls the Server#error method.
 */
static VALUE
rl_error(VALUE self, VALUE message) {
    log_message(&agoo_error_cat, message);
    return Qnil;
}

/* Document-method: fatal
 *
 * call-seq: fatal(message, &block)
 *
 * Calls the Server#fatal method.
 */
static VALUE
rl_fatal(VALUE self, VALUE message) {
    log_message(&agoo_fatal_cat, message);
    exit(0);
    return Qnil;
}

/* Document-class: Agoo::RackLogger
 *
 * Used in a request as the _rack.logger_ attribute.
 */
void
rack_logger_init(VALUE mod) {
    rl_class = rb_define_class_under(mod, "RackLogger", rb_cObject);

    rb_define_method(rl_class, "debug", rl_debug, 1);
    rb_define_method(rl_class, "info", rl_info, 1);
    rb_define_method(rl_class, "warn", rl_warn, 1);
    rb_define_method(rl_class, "error", rl_error, 1);
    rb_define_method(rl_class, "fatal", rl_fatal, 1);
}
