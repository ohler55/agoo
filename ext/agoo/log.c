// Copyright 2018 by Peter Ohler, All Rights Reserved

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "debug.h"
#include "dtime.h"
#include "log.h"

// lower gives faster response but burns more CPU. This is a reasonable compromise.
#define RETRY_SECS	0.0001
#define NOT_WAITING	0
#define WAITING		1
#define NOTIFIED	2
#define RESET_COLOR	"\033[0m"
#define RESET_SIZE	4

static const char	log_name[] = "agoo.log";
static const char	log_prefix[] = "agoo.log.";
static const char	log_format[] = "%s/agoo.log.%d";

static const char	log_pid_name[] = "agoo.log_%d";
static const char	log_pid_prefix[] = "agoo.log_%d.";
static const char	log_pid_format[] = "%s/agoo.log_%d.%d";

static struct _Color	colors[] = {
    { .name = "black",      .ansi = "\033[30;1m" },
    { .name = "red",        .ansi = "\033[31;1m" },
    { .name = "green",      .ansi = "\033[32;1m" },
    { .name = "yellow",     .ansi = "\033[33;1m" },
    { .name = "blue",       .ansi = "\033[34;1m" },
    { .name = "magenta",    .ansi = "\033[35;1m" },
    { .name = "cyan",       .ansi = "\033[36;1m" },
    { .name = "white",      .ansi = "\033[37;1m" },
    { .name = "gray",       .ansi = "\033[37m"   },
    { .name = "dark_red",   .ansi = "\033[31m"   },
    { .name = "dark_green", .ansi = "\033[32m"   },
    { .name = "brown",      .ansi = "\033[33m"   },
    { .name = "dark_blue",  .ansi = "\033[34m"   },
    { .name = "purple",     .ansi = "\033[35m"   },
    { .name = "dark_cyan",  .ansi = "\033[36m"   },
    { .name = NULL,         .ansi = NULL         }
};

static const char	level_chars[] = { 'F', 'E', 'W', 'I', 'D', '?' };

static VALUE	log_mod = Qundef;

struct _Log	the_log = {NULL};
struct _LogCat	fatal_cat;
struct _LogCat	error_cat;
struct _LogCat	warn_cat;
struct _LogCat	info_cat;
struct _LogCat	debug_cat;
struct _LogCat	con_cat;
struct _LogCat	req_cat;
struct _LogCat	resp_cat;
struct _LogCat	eval_cat;
struct _LogCat	push_cat;

static Color
find_color(const char *name) {
    if (NULL != name) {
	for (Color c = colors; NULL != c->name; c++) {
	    if (0 == strcasecmp(c->name, name)) {
		return c;
	    }
	}
    }
    return NULL;
}

static bool
log_queue_empty() {
    LogEntry	head = atomic_load(&the_log.head);
    LogEntry	next = head + 1;

    if (the_log.end <= next) {
	next = the_log.q;
    }
    if (!head->ready && the_log.tail == next) {
	return true;
    }
    return false;
}

static LogEntry
log_queue_pop(double timeout) {
    LogEntry	e = the_log.head;
    LogEntry	next;

    if (e->ready) {
	return e;
    }
    next = the_log.head + 1;
    if (the_log.end <= next) {
	next = the_log.q;
    }
    // If the next is the tail then wait for something to be appended.
    for (int cnt = (int)(timeout / RETRY_SECS); atomic_load(&the_log.tail) == next; cnt--) {
	// TBD poll would be better
	if (cnt <= 0) {
	    return NULL;
	}
	dsleep(RETRY_SECS);
    }
    atomic_store(&the_log.head, next);

    return the_log.head;
}


static int
jwrite(LogEntry e, FILE *file) {
    // TBD make e->what JSON friendly
    return fprintf(file, "{\"when\":%lld.%09lld,\"where\":\"%s\",\"level\":%d,\"what\":\"%s\"}\n",
		   (long long)(e->when / 1000000000LL),
		   (long long)(e->when % 1000000000LL),
		   e->cat->label,
		   e->cat->level,
		   (NULL == e->whatp ? e->what : e->whatp));
}

//I 2015/05/23 11:22:33.123456789 label: The contents of the what field.
static int
classic_write(LogEntry e, FILE *file) {
    time_t	t = (time_t)(e->when / 1000000000LL);
    int		hour = 0;
    int		min = 0;
    int		sec = 0;
    long long	frac = (long long)e->when % 1000000000LL;
    char	levelc = level_chars[e->cat->level];
    int		cnt = 0;
    
    t += the_log.zone;
    if (the_log.day_start <= t && t < the_log.day_end) {
	t -= the_log.day_start;
	hour = (int)(t / 3600);
	min = t % 3600 / 60;
	sec = t % 60;
    } else {
	struct tm	*tm = gmtime(&t);

	hour = tm->tm_hour;
	min = tm->tm_min;
	sec = tm->tm_sec;
	sprintf(the_log.day_buf, "%04d/%02d/%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	the_log.day_start = t - (hour * 3600 + min * 60 + sec);
	the_log.day_end = the_log.day_start + 86400;
    }
    if (the_log.colorize) {
	cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld %s: %s%s\n",
		      e->cat->color->ansi, levelc, the_log.day_buf, hour, min, sec, frac,
		      e->cat->label,
		      (NULL == e->whatp ? e->what : e->whatp),
		      RESET_COLOR);
    } else {
	cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld %s: %s\n",
		       levelc, the_log.day_buf, hour, min, sec, frac,
		       e->cat->label,
		       (NULL == e->whatp ? e->what : e->whatp));
    }
    return cnt;
}

// Remove all file with sequence numbers higher than max_files. max_files is
// max number of archived version. It does not include the primary.
static void
remove_old_logs() {
    struct dirent	*de;
    long		seq;
    char		*end;
    char		path[1024];
    DIR			*dir = opendir(the_log.dir);
    char		prefix[32];
    int			psize;
    char		name[32];

    if (the_log.with_pid) {
	psize = sprintf(prefix, log_pid_prefix, getpid());
	sprintf(name, log_pid_name, getpid());
    } else {
	memcpy(prefix, log_prefix, sizeof(log_prefix));
	psize = (int)sizeof(log_prefix) - 1;
	memcpy(name, log_name, sizeof(log_name));
    }
    while (NULL != (de = readdir(dir))) {
	if ('.' == *de->d_name || '\0' == *de->d_name) {
	    continue;
	}
	if (0 != strncmp(prefix, de->d_name, psize)) {
	    continue;
	}
	// Don't remove the primary log file.
	if (0 == strcmp(name, de->d_name)) {
	    continue;
	}
	seq = strtol(de->d_name + psize, &end, 10);
	if (the_log.max_files < seq) {
	    snprintf(path, sizeof(path), "%s/%s", the_log.dir, de->d_name);
	    remove(path);
	}
    }
    closedir(dir);
}

void
log_rotate() {
    char	from[1024];
    char	to[1024];

    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
    }
    if (the_log.with_pid) {
	char	name[32];

	sprintf(name, log_pid_name, getpid());
	for (int seq = the_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to), log_pid_format, the_log.dir, getpid(), seq + 1);
	    snprintf(from, sizeof(from), log_pid_format, the_log.dir, getpid(), seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to), log_pid_format, the_log.dir, getpid(), 1);
	snprintf(from, sizeof(from), "%s/%s", the_log.dir, name);
    } else {
	for (int seq = the_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to), log_format, the_log.dir, seq + 1);
	    snprintf(from, sizeof(from), log_format, the_log.dir, seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to), log_format, the_log.dir, 1);
	snprintf(from, sizeof(from), "%s/%s", the_log.dir, log_name);
    }
    rename(from, to);

    the_log.file = fopen(from, "w");
    the_log.size = 0;

    remove_old_logs();
}

static void*
loop(void *ctx) {
    LogEntry	e;

    while (!the_log.done || !log_queue_empty()) {
	if (NULL != (e = log_queue_pop(0.5))) {
	    if (the_log.console) {
		if (the_log.classic) {
		    classic_write(e, stdout);
		} else {
		    jwrite(e, stdout);
		}
	    }
	    if (NULL != the_log.file) {
		if (the_log.classic) {
		    the_log.size += classic_write(e, the_log.file);
		} else {
		    the_log.size += jwrite(e, the_log.file);
		}
		if (the_log.max_size <= the_log.size) {
		    log_rotate();
		}
	    }
	    if (NULL != e->whatp) {
		free(e->whatp);
		DEBUG_FREE(mem_log_what, e->whatp)
	    }
	    e->ready = false;
	}
    }
    return NULL;
}

bool
log_flush(double timeout) {
    timeout += dtime();
    
    while (!the_log.done && !log_queue_empty()) {
	if (timeout < dtime()) {
	    return false;
	}
	dsleep(0.001);
    }
    if (NULL != the_log.file) {
	fflush(the_log.file);
    }
    return true;
}

static void
open_log_file() {
    char	path[1024];

    if (the_log.with_pid) {
	snprintf(path, sizeof(path), "%s/%s_%d", the_log.dir, log_name, getpid());
    } else {
	snprintf(path, sizeof(path), "%s/%s", the_log.dir, log_name);
    }
    the_log.file = fopen(path, "a");
    if (NULL == the_log.file) {
	rb_raise(rb_eIOError, "Failed to create '%s'.", path);
    }
    the_log.size = ftell(the_log.file);
    if (the_log.max_size <= the_log.size) {
	log_rotate();
    }
}

void
log_close() {
    the_log.done = true;
    // TBD wake up loop like push does
    log_cat_on(NULL, false);
    if (0 != the_log.thread) {
	pthread_join(the_log.thread, NULL);
	the_log.thread = 0;
    }
    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
    }
    DEBUG_FREE(mem_log_entry, the_log.q)
    free(the_log.q);
    the_log.q = NULL;
    the_log.end = NULL;
    if (0 < the_log.wsock) {
	close(the_log.wsock);
    }
    if (0 < the_log.rsock) {
	close(the_log.rsock);
    }
}

void
log_cat_reg(LogCat cat, const char *label, LogLevel level, const char *color, bool on) {
    strncpy(cat->label, label, sizeof(cat->label));
    cat->label[sizeof(cat->label) - 1] = '\0';
    cat->level = level;
    cat->color = find_color(color);
    cat->on = on;
    cat->next = the_log.cats;
    the_log.cats = cat;
}

void
log_cat_on(const char *label, bool on) {
    LogCat	cat;

    for (cat = the_log.cats; NULL != cat; cat = cat->next) {
	if (NULL == label || 0 == strcasecmp(label, cat->label)) {
	    cat->on = on;
	    break;
	}
    }
}

LogCat
log_cat_find(const char *label) {
    LogCat	cat;

    for (cat = the_log.cats; NULL != cat; cat = cat->next) {
	if (0 == strcasecmp(label, cat->label)) {
	    return cat;
	}
    }
    return NULL;
}

void
log_catv(LogCat cat, const char *fmt, va_list ap) {
    if (cat->on && !the_log.done) {
	LogEntry	e;
	LogEntry	tail;
	int		cnt;
	va_list		ap2;

	va_copy(ap2, ap);
	
	while (atomic_flag_test_and_set(&the_log.push_lock)) {
	    dsleep(RETRY_SECS);
	}
	// Wait for head to move on.
	while (atomic_load(&the_log.head) == the_log.tail) {
	    dsleep(RETRY_SECS);
	}
	e = the_log.tail;
	e->cat = cat;
	{
#ifdef CLOCK_REALTIME
	    struct timespec	ts;
	    
	    clock_gettime(CLOCK_REALTIME, &ts);
	    e->when = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#else
	    struct timeval	tv;
	    struct timezone	tz;

	    gettimeofday(&tv, &tz);

	    e->when = (int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_usec * 1000.0;
#endif
	}
	e->whatp = NULL;
	if ((int)sizeof(e->what) <= (cnt = vsnprintf(e->what, sizeof(e->what), fmt, ap))) {
	    e->whatp = (char*)malloc(cnt + 1);

	    DEBUG_ALLOC(mem_log_what, e->whatp)
    
	    if (NULL != e->whatp) {
		vsnprintf(e->whatp, cnt + 1, fmt, ap2);
	    }
	}
	tail = the_log.tail + 1;
	if (the_log.end <= tail) {
	    tail = the_log.q;
	}
	atomic_store(&the_log.tail, tail);
	atomic_flag_clear(&the_log.push_lock);
	va_end(ap2);

	if (0 != the_log.wsock && WAITING == atomic_load(&the_log.wait_state)) {
	    if (write(the_log.wsock, ".", 1)) {}
	    atomic_store(&the_log.wait_state, NOTIFIED);
	}
    }
}

void
log_cat(LogCat cat, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    log_catv(cat, fmt, ap);
    va_end(ap);
}

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


/* Document-class: Agoo::Log
 *
 * An asynchronous and thread safe logger that includes file rollover and
 * multiple logging categories. It is a feature based logger with a level
 * overlay.
 */
void
log_init(VALUE mod) {
    time_t	t = time(NULL);
    struct tm	*tm = localtime(&t);
    int		qsize = 1024;
    
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

    the_log.cats = NULL;
    *the_log.dir = '\0';
    the_log.file = NULL;
    the_log.max_files = 3;
    the_log.max_size = 100000000; // 100M
    the_log.size = 0;
    the_log.done = false;
    the_log.console = true;
    the_log.classic = true;
    the_log.colorize = true;
    the_log.with_pid = false;
    the_log.zone = (int)(timegm(tm) - t);
    the_log.day_start = 0;
    the_log.day_end = 0;
    *the_log.day_buf = '\0';
    the_log.thread = 0;

    the_log.q = (LogEntry)malloc(sizeof(struct _LogEntry) * qsize);
    DEBUG_ALLOC(mem_log_entry, the_log.q)
    the_log.end = the_log.q + qsize;
    memset(the_log.q, 0, sizeof(struct _LogEntry) * qsize);
    the_log.head = the_log.q;
    the_log.tail = the_log.q + 1;

    atomic_flag_clear(&the_log.push_lock);
    the_log.wait_state = NOT_WAITING;
    // Create when/if needed.
    the_log.rsock = 0;
    the_log.wsock = 0;

    log_cat_reg(&fatal_cat, "FATAL",    FATAL, RED, true);
    log_cat_reg(&error_cat, "ERROR",    ERROR, RED, true);
    log_cat_reg(&warn_cat,  "WARN",     WARN,  YELLOW, true);
    log_cat_reg(&info_cat,  "INFO",     INFO,  GREEN, true);
    log_cat_reg(&debug_cat, "DEBUG",    DEBUG, GRAY, false);
    log_cat_reg(&con_cat,   "connect",  INFO,  GREEN, false);
    log_cat_reg(&req_cat,   "request",  INFO,  CYAN, false);
    log_cat_reg(&resp_cat,  "response", INFO,  DARK_CYAN, false);
    log_cat_reg(&eval_cat,  "eval",     INFO,  BLUE, false);
    log_cat_reg(&push_cat,  "push",     INFO,  DARK_CYAN, false);

    log_start(false);
}

void
log_start(bool with_pid) {
    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
    }
    the_log.with_pid = with_pid;
    if (with_pid && '\0' != *the_log.dir) {
	if (0 != mkdir(the_log.dir, 0770) && EEXIST != errno) {
	    rb_raise(rb_eIOError, "Failed to create '%s'.", the_log.dir);
	}
	open_log_file();
    }
    pthread_create(&the_log.thread, NULL, loop, log);
}
