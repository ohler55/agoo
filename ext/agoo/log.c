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
#include "log.h"

// lower gives faster response but burns more CPU. This is a reasonable compromise.
#define RETRY_SECS	0.0001
#define WAIT_MSECS	100
#define NOT_WAITING	0
#define WAITING		1
#define NOTIFIED	2
#define RESET_COLOR	"\033[0m"
#define RESET_SIZE	4

static struct _agooColor	colors[] = {
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

struct _agooLog	the_log = {NULL};
struct _agooLogCat	fatal_cat;
struct _agooLogCat	error_cat;
struct _agooLogCat	warn_cat;
struct _agooLogCat	info_cat;
struct _agooLogCat	debug_cat;
struct _agooLogCat	con_cat;
struct _agooLogCat	req_cat;
struct _agooLogCat	resp_cat;
struct _agooLogCat	eval_cat;
struct _agooLogCat	push_cat;

agooColor
find_color(const char *name) {
    if (NULL != name) {
	for (agooColor c = colors; NULL != c->name; c++) {
	    if (0 == strcasecmp(c->name, name)) {
		return c;
	    }
	}
    }
    return NULL;
}

static bool
log_queue_empty() {
    agooLogEntry	head = atomic_load(&the_log.head);
    agooLogEntry	next = head + 1;

    if (the_log.end <= next) {
	next = the_log.q;
    }
    if (!head->ready && atomic_load(&the_log.tail) == next) {
	return true;
    }
    return false;
}

static int
log_listen() {
    if (0 == the_log.rsock) {
	int	fd[2];

	if (0 == pipe(fd)) {
	    fcntl(fd[0], F_SETFL, O_NONBLOCK);
	    fcntl(fd[1], F_SETFL, O_NONBLOCK);
	    the_log.rsock = fd[0];
	    the_log.wsock = fd[1];
	}
    }
    atomic_store(&the_log.wait_state, WAITING);
    
    return the_log.rsock;
}

static void
log_release() {
    char	buf[8];

    // clear pipe
    while (0 < read(the_log.rsock, buf, sizeof(buf))) {
    }
    atomic_store(&the_log.wait_state, NOT_WAITING);
}

static agooLogEntry
log_queue_pop(double timeout) {
    agooLogEntry	e = atomic_load(&the_log.head);
    agooLogEntry	next;

    if (e->ready) {
	return e;
    }
    next = (agooLogEntry)atomic_load(&the_log.head) + 1;
    if (the_log.end <= next) {
	next = the_log.q;
    }
    // If the next is the tail then wait for something to be appended.
    for (int cnt = (int)(timeout / (double)WAIT_MSECS * 1000.0); atomic_load(&the_log.tail) == next; cnt--) {
	struct pollfd	pa;

	if (cnt <= 0) {
	    return NULL;
	}
	pa.fd = log_listen();
	pa.events = POLLIN;
	pa.revents = 0;
	if (0 < poll(&pa, 1, WAIT_MSECS)) {
	    log_release();
	}
    }
    atomic_store(&the_log.head, next);

    return next;
}


static int
jwrite(agooLogEntry e, FILE *file) {
    // TBD make e->what JSON friendly
    if (NULL == e->tidp && '\0' == *e->tid) {
	return fprintf(file, "{\"when\":%lld.%09lld,\"where\":\"%s\",\"level\":%d,\"what\":\"%s\"}\n",
		       (long long)(e->when / 1000000000LL),
		       (long long)(e->when % 1000000000LL),
		       e->cat->label,
		       e->cat->level,
		       (NULL == e->whatp ? e->what : e->whatp));
    } else {
	return fprintf(file, "{\"when\":%lld.%09lld,\"tid\":\"%s\",\"where\":\"%s\",\"level\":%d,\"what\":\"%s\"}\n",
		       (long long)(e->when / 1000000000LL),
		       (long long)(e->when % 1000000000LL),
		       (NULL == e->tidp ? e->tid : e->tidp),
		       e->cat->label,
		       e->cat->level,
		       (NULL == e->whatp ? e->what : e->whatp));
    }
}

// I 2015/05/23 11:22:33.123456789 label: The contents of the what field.
// I 2015/05/23 11:22:33.123456789 [tid] label: The contents of the what field.
static int
classic_write(agooLogEntry e, FILE *file) {
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
	if (NULL == e->tidp && '\0' == *e->tid) {
	    cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld %s: %s%s\n",
			  e->cat->color->ansi, levelc, the_log.day_buf, hour, min, sec, frac,
			  e->cat->label,
			  (NULL == e->whatp ? e->what : e->whatp),
			  RESET_COLOR);
	} else {
	    cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld [%s] %s: %s%s\n",
			  e->cat->color->ansi, levelc, the_log.day_buf, hour, min, sec, frac,
			  (NULL == e->tidp ? e->tid : e->tidp),
			  e->cat->label,
			  (NULL == e->whatp ? e->what : e->whatp),
			  RESET_COLOR);
	}
    } else {
	if (NULL == e->tidp && '\0' == *e->tid) {
	    cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld %s: %s\n",
			   levelc, the_log.day_buf, hour, min, sec, frac,
			   e->cat->label,
			   (NULL == e->whatp ? e->what : e->whatp));
	} else {
	    cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld [%s] %s: %s\n",
			   levelc, the_log.day_buf, hour, min, sec, frac,
			   (NULL == e->tidp ? e->tid : e->tidp),
			   e->cat->label,
			   (NULL == e->whatp ? e->what : e->whatp));
	}
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
    char		path[1500];
    DIR			*dir = opendir(the_log.dir);
    char		prefix[64];
    int			psize;
    char		name[64];

    if (the_log.with_pid) {
	psize = sprintf(prefix, "%s_%d.log.", the_log.app, getpid());
	sprintf(name, "%s_%d.log", the_log.app, getpid());
    } else {
	psize = sprintf(prefix, "%s.log.", the_log.app);
	sprintf(name, "%s.log", the_log.app);
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
    char	from[1060];
    char	to[1060];

    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
    }
    if (the_log.with_pid) {
	char	name[32];

	sprintf(name, "%s_%d.log", the_log.app, getpid());
	for (int seq = the_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to) - 1, "%s/%s_%d.log.%d", the_log.dir, the_log.app, getpid(), seq + 1);
	    snprintf(from, sizeof(from) - 1, "%s/%s_%d.log.%d", the_log.dir, the_log.app, getpid(), seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to) - 1, "%s/%s_%d.log.%d", the_log.dir, the_log.app, getpid(), 1);
	snprintf(from, sizeof(from) - 1, "%s/%s", the_log.dir, name);
    } else {
	for (int seq = the_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to) - 1, "%s/%s.log.%d", the_log.dir, the_log.app, seq + 1);
	    snprintf(from, sizeof(from) - 1, "%s/%s.log.%d", the_log.dir, the_log.app, seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to) - 1, "%s/%s.log.%d", the_log.dir, the_log.app, 1);
	snprintf(from, sizeof(from) - 1, "%s/%s.log", the_log.dir, the_log.app);
    }
    rename(from, to);

    the_log.file = fopen(from, "w");
    the_log.size = 0;

    remove_old_logs();
}

static void*
loop(void *ctx) {
    agooLogEntry	e;

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
	    if (NULL != e->tidp) {
		free(e->tidp);
		DEBUG_FREE(mem_log_tid, e->tidp)
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

void
open_log_file() {
    char	path[1500];

    if (the_log.with_pid) {
	snprintf(path, sizeof(path), "%s/%s_%d.log", the_log.dir, the_log.app, getpid());
    } else {
	snprintf(path, sizeof(path), "%s/%s.log", the_log.dir, the_log.app);
    }
    the_log.file = fopen(path, "a");
    if (NULL == the_log.file) {
	struct _agooErr	err;

	err_no(&err, "Failed to create '%s'.", path);
	the_log.on_error(&err);
    }
    the_log.size = ftell(the_log.file);
    if (the_log.max_size <= the_log.size) {
	log_rotate();
    }
    // TBD open rsock and wsock
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
    if (NULL != the_log.q) {
	DEBUG_FREE(mem_log_entry, the_log.q);
	free(the_log.q);
	the_log.q = NULL;
	the_log.end = NULL;
    }
    if (0 < the_log.wsock) {
	close(the_log.wsock);
	the_log.wsock = 0;
    }
    if (0 < the_log.rsock) {
	close(the_log.rsock);
	the_log.rsock = 0;
    }
}

void
log_cat_reg(agooLogCat cat, const char *label, agooLogLevel level, const char *color, bool on) {
    agooLogCat	xcat = log_cat_find(label);
    
    if (NULL != xcat) {
	cat = xcat;
    }
    strncpy(cat->label, label, sizeof(cat->label));
    cat->label[sizeof(cat->label) - 1] = '\0';
    cat->level = level;
    cat->color = find_color(color);
    cat->on = on;
    if (NULL == xcat) {
	cat->next = the_log.cats;
	the_log.cats = cat;
    }
}

void
log_cat_on(const char *label, bool on) {
    agooLogCat	cat;

    for (cat = the_log.cats; NULL != cat; cat = cat->next) {
	if (NULL == label || 0 == strcasecmp(label, cat->label)) {
	    cat->on = on;
	    break;
	}
    }
}

agooLogCat
log_cat_find(const char *label) {
    agooLogCat	cat;

    for (cat = the_log.cats; NULL != cat; cat = cat->next) {
	if (0 == strcasecmp(label, cat->label)) {
	    return cat;
	}
    }
    return NULL;
}

#ifdef CLOCK_REALTIME
static int64_t
now_nano() {
    struct timespec	ts;
	    
    clock_gettime(CLOCK_REALTIME, &ts);

    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}
#else
static int64_t
now_nano() {
    struct timeval	tv;
    struct timezone	tz;

    gettimeofday(&tv, &tz);
    return (int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_usec * 1000.0;
}
#endif

static void
set_entry(agooLogEntry e, agooLogCat cat, const char *tid, const char *fmt, va_list ap) {
    int		cnt;
    va_list	ap2;

    va_copy(ap2, ap);

    e->cat = cat;
    e->when = now_nano();
    e->whatp = NULL;
    if ((int)sizeof(e->what) <= (cnt = vsnprintf(e->what, sizeof(e->what), fmt, ap))) {
	e->whatp = (char*)malloc(cnt + 1);

	DEBUG_ALLOC(mem_log_what, e->whatp)
    
	    if (NULL != e->whatp) {
		vsnprintf(e->whatp, cnt + 1, fmt, ap2);
	    }
    }
    if (NULL != tid) {
	if (strlen(tid) < sizeof(e->tid)) {
	    strcpy(e->tid, tid);
	    e->tidp = NULL;
	} else {
	    e->tidp = strdup(tid);
	    *e->tid = '\0';
	}
    } else {
	e->tidp = NULL;
	*e->tid = '\0';
    }
    va_end(ap2);
}

void
log_catv(agooLogCat cat, const char *tid, const char *fmt, va_list ap) {
    if (cat->on && !the_log.done) {
	agooLogEntry	e;
	agooLogEntry	tail;
	
	while (atomic_flag_test_and_set(&the_log.push_lock)) {
	    dsleep(RETRY_SECS);
	}
	if (0 == the_log.thread) {
	    struct _agooLogEntry	entry;
	    
	    set_entry(&entry, cat, tid, fmt, ap);
	    if (the_log.classic) {
		classic_write(&entry, stdout);
	    } else {
		jwrite(&entry, stdout);
	    }
	    atomic_flag_clear(&the_log.push_lock);

	    return;
	}
	// Wait for head to move on.
	while (atomic_load(&the_log.head) == atomic_load(&the_log.tail)) {
	    dsleep(RETRY_SECS);
	}
	e = atomic_load(&the_log.tail);
	set_entry(e, cat, tid, fmt, ap);
	tail = e + 1;
	if (the_log.end <= tail) {
	    tail = the_log.q;
	}
	atomic_store(&the_log.tail, tail);
	atomic_flag_clear(&the_log.push_lock);

	if (0 != the_log.wsock && WAITING == (int)(long)atomic_load(&the_log.wait_state)) {
	    if (write(the_log.wsock, ".", 1)) {}
	    atomic_store(&the_log.wait_state, NOTIFIED);
	}
    }
}

void
log_cat(agooLogCat cat, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    log_catv(cat, NULL, fmt, ap);
    va_end(ap);
}

void
log_tid_cat(agooLogCat cat, const char *tid, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    log_catv(cat, tid, fmt, ap);
    va_end(ap);
}

void
log_start(bool with_pid) {
    if (0 != the_log.thread) {
	// Already started.
	return;
    }
    if (NULL != the_log.file) {
	fclose(the_log.file);
	the_log.file = NULL;
	// TBD close rsock and wsock
    }
    the_log.with_pid = with_pid;
    if ('\0' != *the_log.dir) {
	if (0 != mkdir(the_log.dir, 0770) && EEXIST != errno) {
	    struct _agooErr	err;

	    err_no(&err, "Failed to create '%s'", the_log.dir);
	    the_log.on_error(&err);
	}
	open_log_file();
    }
    pthread_create(&the_log.thread, NULL, loop, NULL);
}

void
log_init(const char *app) {
    time_t	t = time(NULL);
    struct tm	*tm = localtime(&t);
    int		qsize = 1024;

    strncpy(the_log.app, app, sizeof(the_log.app));
    the_log.app[sizeof(the_log.app) - 1] = '\0';
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

    the_log.q = (agooLogEntry)malloc(sizeof(struct _agooLogEntry) * qsize);
    DEBUG_ALLOC(mem_log_entry, the_log.q)
    the_log.end = the_log.q + qsize;
    memset(the_log.q, 0, sizeof(struct _agooLogEntry) * qsize);
    atomic_init(&the_log.head, the_log.q);
    atomic_init(&the_log.tail, the_log.q + 1);

    agoo_atomic_flag_init(&the_log.push_lock);
    atomic_init(&the_log.wait_state, NOT_WAITING);
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

    //log_start(false);
}
