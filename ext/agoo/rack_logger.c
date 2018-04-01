// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include "rack_logger.h"
#include "debug.h"
#include "log.h"
#include "text.h"

static VALUE	rl_class = Qundef;

typedef struct _RackLogger {
    Server	server;
} *RackLogger;

static void
rack_logger_free(void *ptr) {
    DEBUG_FREE(mem_rack_logger)
    xfree(ptr);
}

VALUE
rack_logger_new(Server server) {
    RackLogger	rl = ALLOC(struct _RackLogger);

    DEBUG_ALLOC(mem_rack_logger)
    rl->server = server;
    
    //return Data_Wrap_Struct(rl_class, NULL, xfree, rl);
    return Data_Wrap_Struct(rl_class, NULL, rack_logger_free, rl);
}

static void
log_message(RackLogger rl, LogLevel level, VALUE message) {
    volatile VALUE	rs = rb_funcall(message, rb_intern("to_s"), 0);
    
    rb_check_type(rs, T_STRING);

    if (rb_block_given_p()) {
	Text		text = text_create(StringValuePtr(rs), (int)RSTRING_LEN(rs));
	volatile VALUE	x = rb_yield_values(0);

	if (Qnil != x) {
	    rs = rb_funcall(x, rb_intern("to_s"), 0);
	    text = text_append(text, ": ", 2);
	    text = text_append(text, StringValuePtr(rs), (int)RSTRING_LEN(rs));
	}
	switch (level) {
	case FATAL:
	    log_cat(&rl->server->error_cat, "%s", text->text);
	    exit(0);
	    break;
	case ERROR:
	    log_cat(&rl->server->error_cat, "%s", text->text);
	    break;
	case WARN:
	    log_cat(&rl->server->warn_cat, "%s", text->text);
	    break;
	case INFO:
	    log_cat(&rl->server->info_cat, "%s", text->text);
	    break;
	case DEBUG:
	    log_cat(&rl->server->debug_cat, "%s", text->text);
	    break;
	default:
	    break;
	}
	text_release(text);
    } else {
	switch (level) {
	case FATAL:
	    log_cat(&rl->server->error_cat, "%s", StringValuePtr(rs));
	    exit(0);
	    break;
	case ERROR:
	    log_cat(&rl->server->error_cat, "%s", StringValuePtr(rs));
	    break;
	case WARN:
	    log_cat(&rl->server->warn_cat, "%s", StringValuePtr(rs));
	    break;
	case INFO:
	    log_cat(&rl->server->info_cat, "%s", StringValuePtr(rs));
	    break;
	case DEBUG:
	    log_cat(&rl->server->debug_cat, "%s", StringValuePtr(rs));
	    break;
	default:
	    break;
	}
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
    log_message((RackLogger)DATA_PTR(self), DEBUG, message);

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
    log_message((RackLogger)DATA_PTR(self), INFO, message);

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
    log_message((RackLogger)DATA_PTR(self), WARN, message);

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
    log_message((RackLogger)DATA_PTR(self), ERROR, message);

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
    log_message((RackLogger)DATA_PTR(self), FATAL, message);

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
