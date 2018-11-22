// Copyright 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_LOG_H
#define AGOO_LOG_H

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "atomic.h"
#include "err.h"

typedef enum {
    AGOO_FATAL		= 0,
    AGOO_ERROR		= 1,
    AGOO_WARN		= 2,
    AGOO_INFO		= 3,
    AGOO_DEBUG		= 4,
    AGOO_UNKNOWN	= 5,
} agooLogLevel;

typedef struct _agooColor {
    const char	*name;
    const char	*ansi;
} *agooColor;

#define AGOO_BLACK	"black"
#define AGOO_RED	"red"
#define AGOO_GREEN	"green"
#define AGOO_YELLOW	"yellow"
#define AGOO_BLUE	"blue"
#define AGOO_MAGENTA	"magenta"
#define AGOO_CYAN	"cyan"
#define AGOO_WHITE	"white"
#define AGOO_GRAY	"gray"
#define AGOO_DARK_RED	"dark_red"
#define AGOO_DARK_GREEN	"dark_green"
#define AGOO_BROWN	"brown"
#define AGOO_DARK_BLUE	"dark_blue"
#define AGOO_PURPLE	"purple"
#define AGOO_DARK_CYAN	"dark_cyan"

typedef struct _agooLog	*agooLog;

typedef struct _agooLogCat {
    struct _agooLogCat	*next;
    char		label[32];
    agooColor		color;
    int			level;
    bool		on;
} *agooLogCat;

typedef struct _agooLogEntry {
    agooLogCat		cat;
    int64_t		when; // nano UTC
    char		*whatp;
    char		what[104];
    volatile bool	ready;
    char		*tidp;
    char		tid[40];
} *agooLogEntry;
	
struct _agooLog {
    agooLogCat			cats;
    char			dir[1024];
    char			app[16];
    FILE			*file;    // current output file
    int				max_files;
    int				max_size;
    long			size;     // current file size
    pthread_t			thread;
    volatile bool		done;
    bool			console;  // if true print log message to stdout
    bool			classic;  // classic in stdout
    bool			colorize; // color in stdout
    bool			with_pid;
    int				zone;     // timezone offset from GMT in seconds
    int64_t			day_start;
    int64_t			day_end;
    char			day_buf[16];

    agooLogEntry		q;
    agooLogEntry		end;
    _Atomic(agooLogEntry)	head;
    _Atomic(agooLogEntry)	tail;
    atomic_flag			push_lock;
    atomic_int			wait_state;
    int				rsock;
    int				wsock;

    void			(*on_error)(agooErr err);
};

extern struct _agooLog		agoo_log;
extern struct _agooLogCat	agoo_fatal_cat;
extern struct _agooLogCat	agoo_error_cat;
extern struct _agooLogCat	agoo_warn_cat;
extern struct _agooLogCat	agoo_info_cat;
extern struct _agooLogCat	agoo_debug_cat;
extern struct _agooLogCat	agoo_con_cat;
extern struct _agooLogCat	agoo_req_cat;
extern struct _agooLogCat	agoo_resp_cat;
extern struct _agooLogCat	agoo_eval_cat;
extern struct _agooLogCat	agoo_push_cat;

extern void		agoo_log_init(const char *app);
extern void		agoo_log_open_file();

extern void		agoo_log_close();
extern bool		agoo_log_flush(double timeout);
extern void		agoo_log_rotate();

extern void		agoo_log_cat_reg(agooLogCat cat, const char *label, agooLogLevel level, const char *color, bool on);
extern void		agoo_log_cat_on(const char *label, bool on);
extern agooLogCat	agoo_log_cat_find(const char *label);

// Function to call to make a log entry.
extern void		agoo_log_cat(agooLogCat cat, const char *fmt, ...);
extern void		agoo_log_tid_cat(agooLogCat cat, const char *tid, const char *fmt, ...);
extern void		agoo_log_catv(agooLogCat cat, const char *tid, const char *fmt, va_list ap);

extern void		agoo_log_start(bool with_pid);

extern agooColor	find_color(const char *name);

#endif /* AGOO_LOG_H */
