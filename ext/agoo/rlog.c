// Copyright 2018 by Peter Ohler, All Rights Reserved

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "dtime.h"
#include "rlog.h"

static VALUE	log_mod = Qundef;

/* Document-method: configure
 *
 * call-seq: configure(options)
 *
 * Configures the logger
 *
 * - *options* [_Hash_] server options
 *
 *   - *:dir* [_String_] directory to place log files in. If nil or empty then no log files are written.
 *
 *   - *:console* [_true_|_false_] if true log entry are display on the console.
 *
 *   - *:classic* [_true_|_false_] if true log entry follow a classic format. If false log entries are JSON.
 *
 *   - *:colorize* [_true_|_false_] if true log entries are colorized.
 *
 *   - *:states* [_Hash_] a map of logging categories and whether they should be on or off. Categories are:
 *     - *:ERROR* errors
 *     - *:WARN* warnings
 *     - *:INFO* infomational
 *     - *:DEBUG* debugging
 *     - *:connect* openning and closing of connections
 *     - *:request* requests
 *     - *:response* responses
 *     - *:eval* handler evaluationss
 *     - *:push* writes to WebSocket or SSE connection
 */
static VALUE
rlog_configure(VALUE self, VALUE options) {
    if (Qnil != options) {
	VALUE	v;

	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("dir"))))) {
	    rb_check_type(v, T_STRING);
	    strncpy(the_log.dir, StringValuePtr(v), sizeof(the_log.dir));
	    the_log.dir[sizeof(the_log.dir) - 1] = '\0';
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("max_files"))))) {
	    int	max = FIX2INT(v);

	    if (1 <= max || max < 100) {
		the_log.max_files = max;
	    } else {
		rb_raise(rb_eArgError, "max_files must be between 1 and 100.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("max_size"))))) {
	    int	max = FIX2INT(v);

	    if (1 <= max) {
		the_log.max_size = max;
	    } else {
		rb_raise(rb_eArgError, "max_size must be 1 or more.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("console"))))) {
	    the_log.console = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("classic"))))) {
	    the_log.classic = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("colorize"))))) {
	    the_log.colorize = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("states"))))) {
	    if (T_HASH == rb_type(v)) {
		LogCat	cat = the_log.cats;
		VALUE	cv;
		
		for (; NULL != cat; cat = cat->next) {
		    if (Qnil != (cv = rb_hash_lookup(v, ID2SYM(rb_intern(cat->label))))) {
			if (Qtrue == cv) {
			    cat->on = true;
			} else if (Qfalse == cv) {
			    cat->on = false;
			}
		    }
		}
	    } else {
		rb_raise(rb_eArgError, "states must be a Hash.");
	    }
	}
    }
    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
    }
    if ('\0' != *the_log.dir) {
	if (0 != mkdir(the_log.dir, 0770) && EEXIST != errno) {
	    rb_raise(rb_eIOError, "Failed to create '%s'.", the_log.dir);
	}
	open_log_file();
    }
    return Qnil;
}

/* Document-method: shutdown
 *
 * call-seq: shutdown()
 *
 * Shutdown the logger. Writes are flushed before shutting down.
 */
static VALUE
rlog_shutdown(VALUE self) {
    log_close();
    return Qnil;
}

/* Document-method: error?
 *
 * call-seq: error?()
 *
 * Returns true is errors are being logged.
 */
static VALUE
rlog_errorp(VALUE self) {
    return error_cat.on ? Qtrue : Qfalse;
}

/* Document-method: warn?
 *
 * call-seq: warn?()
 *
 * Returns true is warnings are being logged.
 */
static VALUE
rlog_warnp(VALUE self) {
    return warn_cat.on ? Qtrue : Qfalse;
}

/* Document-method: info?
 *
 * call-seq: info?()
 *
 * Returns true is info entries are being logged.
 */
static VALUE
rlog_infop(VALUE self) {
    return info_cat.on ? Qtrue : Qfalse;
}

/* Document-method: debug?
 *
 * call-seq: debug?()
 *
 * Returns true is debug entries are being logged.
 */
static VALUE
rlog_debugp(VALUE self) {
    return debug_cat.on ? Qtrue : Qfalse;
}

/* Document-method: error
 *
 * call-seq: error(msg)
 *
 * Log an error message.
 */
static VALUE
rlog_error(VALUE self, VALUE msg) {
    log_cat(&error_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: warn
 *
 * call-seq: warn(msg)
 *
 * Log a warn message.
 */
static VALUE
rlog_warn(VALUE self, VALUE msg) {
    log_cat(&warn_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: info
 *
 * call-seq: info(msg)
 *
 * Log an info message.
 */
static VALUE
rlog_info(VALUE self, VALUE msg) {
    log_cat(&info_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: debug
 *
 * call-seq: debug(msg)
 *
 * Log a debug message.
 */
static VALUE
rlog_debug(VALUE self, VALUE msg) {
    log_cat(&debug_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: color
 *
 * call-seq: color(label)
 *
 * Returns the current color name as a Symbol for the specified label.
 */
static VALUE
rlog_color_get(VALUE self, VALUE label) {
    LogCat	cat = log_cat_find(StringValuePtr(label));

    if (NULL == cat) {
	return Qnil;
    }
    return ID2SYM(rb_intern(cat->color->name));
}

/* Document-method: set_color
 *
 * call-seq: set_color(label, color_symbol)
 *
 * Sets color of the category associated with a label. Valid colors are
 * :black, :red, :green, :yellow, :blue, :magenta, :cyan, :white, :gray,
 * :dark_red, :dark_green, :brown, :dark_blue, :purple, and :dark_cyan.
 */
static VALUE
rlog_color_set(VALUE self, VALUE label, VALUE color) {
    const char	*label_str = StringValuePtr(label);
    const char	*color_name = StringValuePtr(color);
    LogCat	cat = log_cat_find(label_str);
    Color	c = find_color(color_name);

    if (NULL == cat) {
	rb_raise(rb_eArgError, "%s is not a valid category.", label_str);
    }
    if (NULL == c) {
	rb_raise(rb_eArgError, "%s is not a valid color.", color_name);
    }
    cat->color = c;
    
    return Qnil;
}

/* Document-method: state
 *
 * call-seq: state(label)
 *
 * Returns the current state of the category identified by the specified
 * label.
 */
static VALUE
rlog_on_get(VALUE self, VALUE label) {
    LogCat	cat = log_cat_find(StringValuePtr(label));

    if (NULL == cat) {
	return Qfalse;
    }
    return cat->on ? Qtrue : Qfalse;
}

/* Document-method: set_state
 *
 * call-seq: set_state(label, state)
 *
 * Sets state of the category associated with a label.
 */
static VALUE
rlog_on_set(VALUE self, VALUE label, VALUE state) {
    const char	*label_str = StringValuePtr(label);
    LogCat	cat = log_cat_find(label_str);

    if (NULL == cat) {
	rb_raise(rb_eArgError, "%s is not a valid category.", label_str);
    }
    cat->on = (Qtrue == state);
    
    return cat->on ? Qtrue : Qfalse;
}

/* Document-method: log
 *
 * call-seq: log[label] = msg
 *
 * Log a message in the specified category.
 */
static VALUE
rlog_log(VALUE self, VALUE label, VALUE msg) {
    const char	*label_str = StringValuePtr(label);
    LogCat	cat = log_cat_find(label_str);

    if (NULL == cat) {
	rb_raise(rb_eArgError, "%s is not a valid category.", label_str);
    }
    log_cat(cat, "%s", StringValuePtr(msg));
    
    return Qnil;
}

/* Document-method: flush
 *
 * call-seq: flush
 *
 * Flush the log queue and write all entries to disk or the console. The call
 * waits for the flush to complete or the timeout to be exceeded.
 */
static VALUE
rlog_flush(VALUE self, VALUE to) {
    double	timeout = NUM2DBL(to);
    
    if (!log_flush(timeout)) {
	rb_raise(rb_eStandardError, "timed out waiting for log flush.");
    }
    return Qnil;
}

/* Document-method: rotate
 *
 * call-seq: rotate()
 *
 * Rotate the log files.
 */
static VALUE
rlog_rotate(VALUE self) {
    log_rotate();
    return Qnil;
}

/* Document-method: console=
 *
 * call-seq: console=(on)
 *
 * If on then log output also goes to the console.
 */
static VALUE
rlog_console(VALUE self, VALUE on) {
    the_log.console = (Qtrue == on);
    return Qnil;
}

/* Document-method: classic
 *
 * call-seq: classic()
 *
 * Set the log format to classic format.
 */
static VALUE
rlog_classic(VALUE self) {
    the_log.classic = true;
    return Qnil;
}

/* Document-method: json
 *
 * call-seq: json()
 *
 * Set the log format to JSON format.
 */
static VALUE
rlog_json(VALUE self) {
    the_log.classic = false;
    return Qnil;
}

/* Document-method: max_size
 *
 * call-seq: max_size(size)
 *
 * Maximum log files size is reset.
 */
static VALUE
rlog_max_size(VALUE self, VALUE rmax) {
    int	max = FIX2INT(rmax);

    if (1 <= max) {
	the_log.max_size = max;
    } else {
	rb_raise(rb_eArgError, "max_size must be 1 or more.");
    }
    return Qnil;
}

/* Document-method: max_files
 *
 * call-seq: max_files(max)
 *
 * Maximum log files files is reset.
 */
static VALUE
rlog_max_files(VALUE self, VALUE rmax) {
    int	max = FIX2INT(rmax);

    if (1 <= max || max < 100) {
	the_log.max_files = max;
    } else {
	rb_raise(rb_eArgError, "max_files must be between 1 and 100.");
    }
    return Qnil;
}

static void
on_error(Err err) {
    rb_raise(rb_eStandardError, "%s", err->msg);
}

/* Document-class: Agoo::Log
 *
 * An asynchronous and thread safe logger that includes file rollover and
 * multiple logging categories. It is a feature based logger with a level
 * overlay.
 */
void
rlog_init(VALUE mod) {
    log_mod = rb_define_module_under(mod, "Log");

    rb_define_module_function(log_mod, "configure", rlog_configure, 1);
    rb_define_module_function(log_mod, "shutdown", rlog_shutdown, 0);

    rb_define_module_function(log_mod, "error?", rlog_errorp, 0);
    rb_define_module_function(log_mod, "warn?", rlog_warnp, 0);
    rb_define_module_function(log_mod, "info?", rlog_infop, 0);
    rb_define_module_function(log_mod, "debug?", rlog_debugp, 0);

    rb_define_module_function(log_mod, "error", rlog_error, 1);
    rb_define_module_function(log_mod, "warn", rlog_warn, 1);
    rb_define_module_function(log_mod, "info", rlog_info, 1);
    rb_define_module_function(log_mod, "debug", rlog_debug, 1);

    // TBD maybe in a future version
    //rb_define_module_function(log_mod, "register", rlog_register, 2);

    rb_define_module_function(log_mod, "color", rlog_color_get, 1);
    rb_define_module_function(log_mod, "set_color", rlog_color_set, 2);

    rb_define_module_function(log_mod, "state", rlog_on_get, 1);
    rb_define_module_function(log_mod, "set_state", rlog_on_set, 2);

    rb_define_module_function(log_mod, "log", rlog_log, 2);

    rb_define_module_function(log_mod, "flush", rlog_flush, 1);
    rb_define_module_function(log_mod, "rotate", rlog_rotate, 0);
    rb_define_module_function(log_mod, "console=", rlog_console, 1);
    rb_define_module_function(log_mod, "classic", rlog_classic, 0);
    rb_define_module_function(log_mod, "json", rlog_json, 0);
    rb_define_module_function(log_mod, "max_size=", rlog_max_size, 1);
    rb_define_module_function(log_mod, "max_files=", rlog_max_files, 1);

    the_log.on_error = on_error;
    
    log_init();
}
