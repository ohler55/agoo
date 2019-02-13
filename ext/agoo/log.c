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
#include "sectime.h"

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

struct _agooLog	agoo_log = {NULL};
struct _agooLogCat	agoo_fatal_cat;
struct _agooLogCat	agoo_error_cat;
struct _agooLogCat	agoo_warn_cat;
struct _agooLogCat	agoo_info_cat;
struct _agooLogCat	agoo_debug_cat;
struct _agooLogCat	agoo_con_cat;
struct _agooLogCat	agoo_req_cat;
struct _agooLogCat	agoo_resp_cat;
struct _agooLogCat	agoo_eval_cat;
struct _agooLogCat	agoo_push_cat;

agooColor
find_color(const char *name) {
    if (NULL != name) {
	agooColor	c;

	for (c = colors; NULL != c->name; c++) {
	    if (0 == strcasecmp(c->name, name)) {
		return c;
	    }
	}
    }
    return NULL;
}

static bool
agoo_log_queue_empty() {
    agooLogEntry	head = atomic_load(&agoo_log.head);
    agooLogEntry	next = head + 1;

    if (agoo_log.end <= next) {
	next = agoo_log.q;
    }
    if (!head->ready && atomic_load(&agoo_log.tail) == next) {
	return true;
    }
    return false;
}

static int
agoo_log_listen() {
    if (0 == agoo_log.rsock) {
	int	fd[2];

	if (0 == pipe(fd)) {
	    fcntl(fd[0], F_SETFL, O_NONBLOCK);
	    fcntl(fd[1], F_SETFL, O_NONBLOCK);
	    agoo_log.rsock = fd[0];
	    agoo_log.wsock = fd[1];
	}
    }
    atomic_store(&agoo_log.wait_state, WAITING);

    return agoo_log.rsock;
}

static void
agoo_log_release() {
    char	buf[8];

    // clear pipe
    while (0 < read(agoo_log.rsock, buf, sizeof(buf))) {
    }
    atomic_store(&agoo_log.wait_state, NOT_WAITING);
}

static agooLogEntry
agoo_log_queue_pop(double timeout) {
    agooLogEntry	e = atomic_load(&agoo_log.head);
    agooLogEntry	next;
    int			cnt;

    if (e->ready) {
	return e;
    }
    next = (agooLogEntry)atomic_load(&agoo_log.head) + 1;
    if (agoo_log.end <= next) {
	next = agoo_log.q;
    }
    // If the next is the tail then wait for something to be appended.
    for (cnt = (int)(timeout / (double)WAIT_MSECS * 1000.0); atomic_load(&agoo_log.tail) == next; cnt--) {
	struct pollfd	pa;

	if (cnt <= 0) {
	    return NULL;
	}
	pa.fd = agoo_log_listen();
	pa.events = POLLIN;
	pa.revents = 0;
	if (0 < poll(&pa, 1, WAIT_MSECS)) {
	    agoo_log_release();
	}
    }
    atomic_store(&agoo_log.head, next);

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

    t += agoo_log.zone;
    if (agoo_log.day_start <= t && t < agoo_log.day_end) {
	t -= agoo_log.day_start;
	hour = (int)(t / 3600);
	min = t % 3600 / 60;
	sec = t % 60;
    } else {
	struct _agooTime	at;

	agoo_sectime(t, &at);

	hour = at.hour;
	min = at.min;
	sec = at.sec;
	sprintf(agoo_log.day_buf, "%04d/%02d/%02d ", at.year, at.mon, at.day);
	agoo_log.day_start = t - (hour * 3600 + min * 60 + sec);
	agoo_log.day_end = agoo_log.day_start + 86400;
    }
    if (agoo_log.colorize) {
	if (NULL == e->tidp && '\0' == *e->tid) {
	    cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld %s: %s%s\n",
			  e->cat->color->ansi, levelc, agoo_log.day_buf, hour, min, sec, frac,
			  e->cat->label,
			  (NULL == e->whatp ? e->what : e->whatp),
			  RESET_COLOR);
	} else {
	    cnt = fprintf(file, "%s%c %s%02d:%02d:%02d.%09lld [%s] %s: %s%s\n",
			  e->cat->color->ansi, levelc, agoo_log.day_buf, hour, min, sec, frac,
			  (NULL == e->tidp ? e->tid : e->tidp),
			  e->cat->label,
			  (NULL == e->whatp ? e->what : e->whatp),
			  RESET_COLOR);
	}
    } else {
	if (NULL == e->tidp && '\0' == *e->tid) {
	    cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld %s: %s\n",
			   levelc, agoo_log.day_buf, hour, min, sec, frac,
			   e->cat->label,
			   (NULL == e->whatp ? e->what : e->whatp));
	} else {
	    cnt += fprintf(file, "%c %s%02d:%02d:%02d.%09lld [%s] %s: %s\n",
			   levelc, agoo_log.day_buf, hour, min, sec, frac,
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
    DIR			*dir = opendir(agoo_log.dir);
    char		prefix[64];
    int			psize;
    char		name[64];

    if (agoo_log.with_pid) {
	psize = sprintf(prefix, "%s_%d.log.", agoo_log.app, getpid());
	sprintf(name, "%s_%d.log", agoo_log.app, getpid());
    } else {
	psize = sprintf(prefix, "%s.log.", agoo_log.app);
	sprintf(name, "%s.log", agoo_log.app);
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
	if (agoo_log.max_files < seq) {
	    snprintf(path, sizeof(path), "%s/%s", agoo_log.dir, de->d_name);
	    remove(path);
	}
    }
    closedir(dir);
}

void
agoo_log_rotate() {
    char	from[1060];
    char	to[1060];
    int		seq;

    if (NULL != agoo_log.file) {
	fclose(agoo_log.file);
	agoo_log.file = NULL;
    }
    if (agoo_log.with_pid) {
	char	name[32];

	sprintf(name, "%s_%d.log", agoo_log.app, getpid());
	for (seq = agoo_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to) - 1, "%s/%s_%d.log.%d", agoo_log.dir, agoo_log.app, getpid(), seq + 1);
	    snprintf(from, sizeof(from) - 1, "%s/%s_%d.log.%d", agoo_log.dir, agoo_log.app, getpid(), seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to) - 1, "%s/%s_%d.log.%d", agoo_log.dir, agoo_log.app, getpid(), 1);
	snprintf(from, sizeof(from) - 1, "%s/%s", agoo_log.dir, name);
    } else {
	for (seq = agoo_log.max_files; 0 < seq; seq--) {
	    snprintf(to, sizeof(to) - 1, "%s/%s.log.%d", agoo_log.dir, agoo_log.app, seq + 1);
	    snprintf(from, sizeof(from) - 1, "%s/%s.log.%d", agoo_log.dir, agoo_log.app, seq);
	    rename(from, to);
	}
	snprintf(to, sizeof(to) - 1, "%s/%s.log.%d", agoo_log.dir, agoo_log.app, 1);
	snprintf(from, sizeof(from) - 1, "%s/%s.log", agoo_log.dir, agoo_log.app);
    }
    rename(from, to);

    agoo_log.file = fopen(from, "w");
    agoo_log.size = 0;

    remove_old_logs();
}

static void*
loop(void *ctx) {
    agooLogEntry	e;

    while (!agoo_log.done || !agoo_log_queue_empty()) {
	if (NULL != (e = agoo_log_queue_pop(0.5))) {
	    if (agoo_log.console) {
		if (agoo_log.classic) {
		    classic_write(e, stdout);
		} else {
		    jwrite(e, stdout);
		}
	    }
	    if (NULL != agoo_log.file) {
		if (agoo_log.classic) {
		    agoo_log.size += classic_write(e, agoo_log.file);
		} else {
		    agoo_log.size += jwrite(e, agoo_log.file);
		}
		if (agoo_log.max_size <= agoo_log.size) {
		    agoo_log_rotate();
		}
	    }
	    AGOO_FREE(e->whatp);
	    AGOO_FREE(e->tidp);
	    e->ready = false;
	}
    }
    return NULL;
}

bool
agoo_log_flush(double timeout) {
    timeout += dtime();

    while (!agoo_log.done && !agoo_log_queue_empty()) {
	if (timeout < dtime()) {
	    return false;
	}
	dsleep(0.001);
    }
    if (NULL != agoo_log.file) {
	fflush(agoo_log.file);
    }
    return true;
}

void
agoo_log_open_file() {
    char	path[1500];

    if (agoo_log.with_pid) {
	snprintf(path, sizeof(path), "%s/%s_%d.log", agoo_log.dir, agoo_log.app, getpid());
    } else {
	snprintf(path, sizeof(path), "%s/%s.log", agoo_log.dir, agoo_log.app);
    }
    agoo_log.file = fopen(path, "a");
    if (NULL == agoo_log.file) {
	struct _agooErr	err;

	agoo_err_no(&err, "Failed to create '%s'.", path);
	agoo_log.on_error(&err);
    }
    agoo_log.size = ftell(agoo_log.file);
    if (agoo_log.max_size <= agoo_log.size) {
	agoo_log_rotate();
    }
    // TBD open rsock and wsock
}

void
agoo_log_close() {
    agoo_log.done = true;
    // TBD wake up loop like push does
    agoo_log_cat_on(NULL, false);
    if (0 != agoo_log.thread) {
	pthread_join(agoo_log.thread, NULL);
	agoo_log.thread = 0;
    }
    if (NULL != agoo_log.file) {
	fclose(agoo_log.file);
	agoo_log.file = NULL;
    }
    if (NULL != agoo_log.q) {
	AGOO_FREE(agoo_log.q);
	agoo_log.q = NULL;
	agoo_log.end = NULL;
    }
    if (0 < agoo_log.wsock) {
	close(agoo_log.wsock);
	agoo_log.wsock = 0;
    }
    if (0 < agoo_log.rsock) {
	close(agoo_log.rsock);
	agoo_log.rsock = 0;
    }
}

void
agoo_log_cat_reg(agooLogCat cat, const char *label, agooLogLevel level, const char *color, bool on) {
    agooLogCat	xcat = agoo_log_cat_find(label);

    if (NULL != xcat) {
	cat = xcat;
    }
    strncpy(cat->label, label, sizeof(cat->label));
    cat->label[sizeof(cat->label) - 1] = '\0';
    cat->level = level;
    cat->color = find_color(color);
    cat->on = on;
    if (NULL == xcat) {
	cat->next = agoo_log.cats;
	agoo_log.cats = cat;
    }
}

void
agoo_log_cat_on(const char *label, bool on) {
    agooLogCat	cat;

    for (cat = agoo_log.cats; NULL != cat; cat = cat->next) {
	if (NULL == label || 0 == strcasecmp(label, cat->label)) {
	    cat->on = on;
	    break;
	}
    }
}

agooLogCat
agoo_log_cat_find(const char *label) {
    agooLogCat	cat;

    for (cat = agoo_log.cats; NULL != cat; cat = cat->next) {
	if (0 == strcasecmp(label, cat->label)) {
	    return cat;
	}
    }
    return NULL;
}

#ifdef CLOCK_REALTIME
int64_t
agoo_now_nano() {
    struct timespec	ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}
#else
int64_t
agoo_now_nano() {
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
    e->when = agoo_now_nano();
    e->whatp = NULL;
    if ((int)sizeof(e->what) <= (cnt = vsnprintf(e->what, sizeof(e->what), fmt, ap))) {
	e->whatp = (char*)AGOO_MALLOC(cnt + 1);

	if (NULL != e->whatp) {
	    vsnprintf(e->whatp, cnt + 1, fmt, ap2);
	}
    }
    if (NULL != tid) {
	e->tidp = NULL;
	if (strlen(tid) < sizeof(e->tid)) {
	    strcpy(e->tid, tid);
	} else {
	    if (NULL != (e->tidp = AGOO_STRDUP(tid))) {
		*e->tid = '\0';
	    }
	}
    } else {
	e->tidp = NULL;
	*e->tid = '\0';
    }
    va_end(ap2);
}

void
agoo_log_catv(agooLogCat cat, const char *tid, const char *fmt, va_list ap) {
    if (cat->on && !agoo_log.done) {
	agooLogEntry	e;
	agooLogEntry	tail;

	while (atomic_flag_test_and_set(&agoo_log.push_lock)) {
	    dsleep(RETRY_SECS);
	}
	if (0 == agoo_log.thread) {
	    struct _agooLogEntry	entry;

	    set_entry(&entry, cat, tid, fmt, ap);
	    if (agoo_log.classic) {
		classic_write(&entry, stdout);
	    } else {
		jwrite(&entry, stdout);
	    }
	    atomic_flag_clear(&agoo_log.push_lock);

	    return;
	}
	// Wait for head to move on.
	while (atomic_load(&agoo_log.head) == atomic_load(&agoo_log.tail)) {
	    dsleep(RETRY_SECS);
	}
	e = atomic_load(&agoo_log.tail);
	set_entry(e, cat, tid, fmt, ap);
	tail = e + 1;
	if (agoo_log.end <= tail) {
	    tail = agoo_log.q;
	}
	atomic_store(&agoo_log.tail, tail);
	atomic_flag_clear(&agoo_log.push_lock);

	if (0 != agoo_log.wsock && WAITING == (int)(long)atomic_load(&agoo_log.wait_state)) {
	    if (write(agoo_log.wsock, ".", 1)) {}
	    atomic_store(&agoo_log.wait_state, NOTIFIED);
	}
    }
}

void
agoo_log_cat(agooLogCat cat, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    agoo_log_catv(cat, NULL, fmt, ap);
    va_end(ap);
}

void
agoo_log_tid_cat(agooLogCat cat, const char *tid, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    agoo_log_catv(cat, tid, fmt, ap);
    va_end(ap);
}

int
agoo_log_start(agooErr err, bool with_pid) {
    int	stat;

    if (0 != agoo_log.thread) {
	// Already started.
	return AGOO_ERR_OK;
    }
    if (NULL != agoo_log.file) {
	fclose(agoo_log.file);
	agoo_log.file = NULL;
	// TBD close rsock and wsock
    }
    agoo_log.with_pid = with_pid;
    if ('\0' != *agoo_log.dir) {
	if (0 != mkdir(agoo_log.dir, 0770) && EEXIST != errno) {
	    struct _agooErr	err;

	    agoo_err_no(&err, "Failed to create '%s'", agoo_log.dir);
	    agoo_log.on_error(&err);
	}
	agoo_log_open_file();
    }
    if (0 != (stat = pthread_create(&agoo_log.thread, NULL, loop, NULL))) {
	return agoo_err_set(err, stat, "Failed to create log thread. %s", strerror(stat));
    }
    return AGOO_ERR_OK;
}

int
agoo_log_init(agooErr err, const char *app) {
    time_t	t = time(NULL);
    struct tm	*tm = localtime(&t);
    int		qsize = 1024;

    strncpy(agoo_log.app, app, sizeof(agoo_log.app));
    agoo_log.app[sizeof(agoo_log.app) - 1] = '\0';
    agoo_log.cats = NULL;
    *agoo_log.dir = '\0';
    agoo_log.file = NULL;
    agoo_log.max_files = 3;
    agoo_log.max_size = 100000000; // 100M
    agoo_log.size = 0;
    agoo_log.done = false;
    agoo_log.console = true;
    agoo_log.classic = true;
    agoo_log.colorize = true;
    agoo_log.with_pid = false;
    agoo_log.zone = (int)(timegm(tm) - t);
    agoo_log.day_start = 0;
    agoo_log.day_end = 0;
    *agoo_log.day_buf = '\0';
    agoo_log.thread = 0;

    if (NULL == (agoo_log.q = (agooLogEntry)AGOO_CALLOC(qsize, sizeof(struct _agooLogEntry)))) {
	return AGOO_ERR_MEM(err, "Log Queue");
    }
    agoo_log.end = agoo_log.q + qsize;
    atomic_init(&agoo_log.head, agoo_log.q);
    atomic_init(&agoo_log.tail, agoo_log.q + 1);

    agoo_atomic_flag_init(&agoo_log.push_lock);
    atomic_init(&agoo_log.wait_state, NOT_WAITING);
    // Create when/if needed.
    agoo_log.rsock = 0;
    agoo_log.wsock = 0;

    agoo_log_cat_reg(&agoo_fatal_cat, "FATAL",    AGOO_FATAL, AGOO_RED, true);
    agoo_log_cat_reg(&agoo_error_cat, "ERROR",    AGOO_ERROR, AGOO_RED, true);
    agoo_log_cat_reg(&agoo_warn_cat,  "WARN",     AGOO_WARN,  AGOO_YELLOW, true);
    agoo_log_cat_reg(&agoo_info_cat,  "INFO",     AGOO_INFO,  AGOO_GREEN, true);
    agoo_log_cat_reg(&agoo_debug_cat, "DEBUG",    AGOO_DEBUG, AGOO_GRAY, false);
    agoo_log_cat_reg(&agoo_con_cat,   "connect",  AGOO_INFO,  AGOO_GREEN, false);
    agoo_log_cat_reg(&agoo_req_cat,   "request",  AGOO_INFO,  AGOO_CYAN, false);
    agoo_log_cat_reg(&agoo_resp_cat,  "response", AGOO_INFO,  AGOO_DARK_CYAN, false);
    agoo_log_cat_reg(&agoo_eval_cat,  "eval",     AGOO_INFO,  AGOO_BLUE, false);
    agoo_log_cat_reg(&agoo_push_cat,  "push",     AGOO_INFO,  AGOO_DARK_CYAN, false);

    //agoo_log_start(false);
    return AGOO_ERR_OK;
}
