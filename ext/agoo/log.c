// Copyright 2018 by Peter Ohler, All Rights Reserved

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
log_queue_empty(Log log) {
    LogEntry	head = atomic_load(&log->head);
    LogEntry	next = head + 1;

    if (log->end <= next) {
	next = log->q;
    }
    if (!head->ready && log->tail == next) {
	return true;
    }
    return false;
}

static LogEntry
log_queue_pop(Log log, double timeout) {
    LogEntry	e = log->head;
    LogEntry	next;

    if (e->ready) {
	return e;
    }
    next = log->head + 1;
    if (log->end <= next) {
	next = log->q;
    }
    // If the next is the tail then wait for something to be appended.
    for (int cnt = (int)(timeout / RETRY_SECS); atomic_load(&log->tail) == next; cnt--) {
	// TBD poll would be better
	if (cnt <= 0) {
	    return NULL;
	}
	dsleep(RETRY_SECS);
    }
    atomic_store(&log->head, next);

    return log->head;
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
classic_write(Log log, LogEntry e, FILE *file) {
    time_t	t = (time_t)(e->when / 1000000000LL);
    int		hour = 0;
    int		min = 0;
    int		sec = 0;
    long long	frac = (long long)e->when % 1000000000LL;
    char	levelc = level_chars[e->cat->level];
    int		cnt = 0;
    
    t += log->zone;
    if (log->day_start <= t && t < log->day_end) {
	t -= log->day_start;
	hour = (int)(t / 3600);
	min = t % 3600 / 60;
	sec = t % 60;
    } else {
	struct tm	*tm = gmtime(&t);

	hour = tm->tm_hour;
	min = tm->tm_min;
	sec = tm->tm_sec;
	sprintf(log->day_buf, "%04d/%02d/%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	log->day_start = t - (hour * 3600 + min * 60 + sec);
	log->day_end = log->day_start + 86400;
    }
    if (log->colorize) {
	cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld %s: %s%s\n",
		      e->cat->color->ansi, levelc, log->day_buf, hour, min, sec, frac,
		      e->cat->label,
		      (NULL == e->whatp ? e->what : e->whatp),
		      RESET_COLOR);
    } else {
	cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld %s: %s\n",
		       levelc, log->day_buf, hour, min, sec, frac,
		       e->cat->label,
		       (NULL == e->whatp ? e->what : e->whatp));
    }
    return cnt;
}

// Remove all file with sequence numbers higher than max_files. max_files is
// max number of archived version. It does not include the primary.
static void
remove_old_logs(Log log) {
    struct dirent	*de;
    long		seq;
    char		*end;
    char		path[1024];
    DIR			*dir = opendir(log->dir);
    
    while (NULL != (de = readdir(dir))) {
	if ('.' == *de->d_name || '\0' == *de->d_name) {
	    continue;
	}
	if (0 != strncmp(log_prefix, de->d_name, sizeof(log_prefix) - 1)) {
	    continue;
	}
	// Don't remove the primary log file.
	if (0 == strcmp(log_name, de->d_name)) {
	    continue;
	}
	seq = strtol(de->d_name + sizeof(log_prefix) - 1, &end, 10);
	if (log->max_files < seq) {
	    snprintf(path, sizeof(path), "%s/%s", log->dir, de->d_name);
	    remove(path);
	}
    }
    closedir(dir);
}

static int
rotate(Err err, Log log) {
    char	from[1024];
    char	to[1024];

    if (NULL != log->file) {
	fclose(log->file);
	log->file = NULL;
    }
    for (int seq = log->max_files; 0 < seq; seq--) {
	snprintf(to, sizeof(to), log_format, log->dir, seq + 1);
	snprintf(from, sizeof(from), log_format, log->dir, seq);
	rename(from, to);
    }
    snprintf(to, sizeof(to), log_format, log->dir, 1);
    snprintf(from, sizeof(from), "%s/%s", log->dir, log_name);
    rename(from, to);

    log->file = fopen(from, "w");
    log->size = 0;

    remove_old_logs(log);

    return ERR_OK;
}

static void*
loop(void *ctx) {
    Log		log = (Log)ctx;
    LogEntry	e;

    while (!log->done || !log_queue_empty(log)) {
	if (NULL != (e = log_queue_pop(log, 0.5))) {
	    if (log->console) {
		if (log->classic) {
		    classic_write(log, e, stdout);
		} else {
		    jwrite(e, stdout);
		}
	    }
	    if (NULL != log->file) {
		if (log->classic) {
		    log->size += classic_write(log, e, log->file);
		} else {
		    log->size += jwrite(e, log->file);
		}
		if (log->max_size <= log->size) {
		    rotate(NULL, log);
		}
	    }
	    if (NULL != e->whatp) {
		free(e->whatp);
		DEBUG_FREE(mem_log_what)
	    }
	    e->ready = false;
	}
    }
    return NULL;
}

bool
log_flush(Log log, double timeout) {
    timeout += dtime();
    
    while (!log->done && !log_queue_empty(log)) {
	if (timeout < dtime()) {
	    return false;
	}
	dsleep(0.001);
    }
    if (NULL != log->file) {
	fflush(log->file);
    }
    return true;
}

static int
configure(Err err, Log log, VALUE options) {
    if (Qnil != options) {
	VALUE	v;

	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_dir"))))) {
	    rb_check_type(v, T_STRING);
	    strncpy(log->dir, StringValuePtr(v), sizeof(log->dir));
	    log->dir[sizeof(log->dir) - 1] = '\0';
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_max_files"))))) {
	    int	max = FIX2INT(v);

	    if (1 <= max || max < 100) {
		log->max_files = max;
	    } else {
		rb_raise(rb_eArgError, "log_max_files must be between 1 and 100.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_max_size"))))) {
	    int	max = FIX2INT(v);

	    if (1 <= max) {
		log->max_size = max;
	    } else {
		rb_raise(rb_eArgError, "log_max_size must be 1 or more.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_console"))))) {
	    log->console = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_classic"))))) {
	    log->classic = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_colorize"))))) {
	    log->colorize = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("log_states"))))) {
	    if (T_HASH == rb_type(v)) {
		LogCat	cat = log->cats;
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
		rb_raise(rb_eArgError, "log_states must be a Hash.");
	    }
	}
    }
    return ERR_OK;
}

static int
open_log_file(Err err, Log log) {
    char	path[1024];

    snprintf(path, sizeof(path), "%s/%s", log->dir, log_name);

    log->file = fopen(path, "a");
    if (NULL == log->file) {
	return err_no(err, "failed to open '%s'.", path);
    }
    log->size = ftell(log->file);
    if (log->max_size <= log->size) {
	return rotate(err, log);
    }
    return ERR_OK;
}

int
log_init(Err err, Log log, VALUE cfg) {
    time_t	t = time(NULL);
    struct tm	*tm = localtime(&t);
    int		qsize = 1024;
    
    //log->cats = NULL; done outside of here
    *log->dir = '\0';
    log->file = NULL;
    log->max_files = 3;
    log->max_size = 100000000; // 100M
    log->size = 0;
    log->done = false;
    log->console = true;
    log->classic = true;
    log->colorize = true;
    log->zone = (int)(timegm(tm) - t);
    log->day_start = 0;
    log->day_end = 0;
    *log->day_buf = '\0';
    log->thread = 0;

    if (ERR_OK != configure(err, log, cfg)) {
	return err->code;
    }
    if ('\0' != *log->dir) {
	if (0 != mkdir(log->dir, 0770) && EEXIST != errno) {
	    return err_no(err, "Failed to create '%s'.", log->dir);
	}
	if (ERR_OK != open_log_file(err, log)) {
	    return err->code;
	}
    }
    log->q = (LogEntry)malloc(sizeof(struct _LogEntry) * qsize);
    DEBUG_ALLOC(mem_log_entry)

    log->end = log->q + qsize;

    memset(log->q, 0, sizeof(struct _LogEntry) * qsize);
    log->head = log->q;
    log->tail = log->q + 1;
    atomic_flag_clear(&log->push_lock);
    log->wait_state = NOT_WAITING;
    // Create when/if needed.
    log->rsock = 0;
    log->wsock = 0;
    
    pthread_create(&log->thread, NULL, loop, log);

    return ERR_OK;
}

void
log_close(Log log) {
    log->done = true;
    // TBD wake up loop like push does
    log_cat_on(log, NULL, false);
    if (0 != log->thread) {
	pthread_join(log->thread, NULL);
	log->thread = 0;
    }
    if (NULL != log->file) {
	fclose(log->file);
	log->file = NULL;
    }
    DEBUG_FREE(mem_log_entry)
    free(log->q);
    log->q = NULL;
    log->end = NULL;
    if (0 < log->wsock) {
	close(log->wsock);
    }
    if (0 < log->rsock) {
	close(log->rsock);
    }
}

void
log_cat_reg(Log log, LogCat cat, const char *label, LogLevel level, const char *color, bool on) {
    cat->log = log;
    strncpy(cat->label, label, sizeof(cat->label));
    cat->label[sizeof(cat->label) - 1] = '\0';
    cat->level = level;
    cat->color = find_color(color);
    cat->on = on;
    cat->next = log->cats;
    log->cats = cat;
}

void
log_cat_on(Log log, const char *label, bool on) {
    LogCat	cat;

    for (cat = log->cats; NULL != cat; cat = cat->next) {
	if (NULL == label || 0 == strcasecmp(label, cat->label)) {
	    cat->on = on;
	    break;
	}
    }
}

LogCat
log_cat_find(Log log, const char *label) {
    LogCat	cat;

    for (cat = log->cats; NULL != cat; cat = cat->next) {
	if (0 == strcasecmp(label, cat->label)) {
	    return cat;
	}
    }
    return NULL;
}

void
log_catv(LogCat cat, const char *fmt, va_list ap) {
    if (cat->on && !cat->log->done) {
	Log		log = cat->log;
	struct timespec	ts;
	LogEntry	e;
	LogEntry	tail;
	int		cnt;
	va_list		ap2;

	va_copy(ap2, ap);
	
	while (atomic_flag_test_and_set(&log->push_lock)) {
	    dsleep(RETRY_SECS);
	}
	// Wait for head to move on.
	while (atomic_load(&log->head) == log->tail) {
	    dsleep(RETRY_SECS);
	}
	// TBD fill in the entry at tail
	clock_gettime(CLOCK_REALTIME, &ts);
	e = log->tail;
	e->cat = cat;
	e->when = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
	e->whatp = NULL;
	if ((int)sizeof(e->what) <= (cnt = vsnprintf(e->what, sizeof(e->what), fmt, ap))) {
	    e->whatp = (char*)malloc(cnt + 1);

	    DEBUG_ALLOC(mem_log_what)
    
	    if (NULL != e->whatp) {
		vsnprintf(e->whatp, cnt + 1, fmt, ap2);
	    }
	}
	tail = log->tail + 1;
	if (log->end <= tail) {
	    tail = log->q;
	}
	atomic_store(&log->tail, tail);
	atomic_flag_clear(&log->push_lock);
	va_end(ap2);

	if (0 != log->wsock && WAITING == atomic_load(&log->wait_state)) {
	    if (write(log->wsock, ".", 1)) {}
	    atomic_store(&log->wait_state, NOTIFIED);
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
