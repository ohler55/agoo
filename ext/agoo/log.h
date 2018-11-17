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
    FATAL	= 0,
    ERROR	= 1,
    WARN	= 2,
    INFO	= 3,
    DEBUG	= 4,
    UNKNOWN	= 5,
} agooLogLevel;

typedef struct _agooColor {
    const char	*name;
    const char	*ansi;
} *agooColor;

#define BLACK		"black"
#define RED		"red"
#define GREEN		"green"
#define YELLOW		"yellow"
#define BLUE		"blue"
#define MAGENTA		"magenta"
#define CYAN		"cyan"
#define WHITE		"white"
#define GRAY		"gray"
#define DARK_RED	"dark_red"
#define DARK_GREEN	"dark_green"
#define BROWN		"brown"
#define DARK_BLUE	"dark_blue"
#define PURPLE		"purple"
#define DARK_CYAN	"dark_cyan"

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
    agooLogCat		cats;
    char		dir[1024];
    char		app[16];
    FILE		*file;    // current output file
    int			max_files;
    int			max_size;
    long		size;     // current file size
    pthread_t		thread;
    volatile bool	done;
    bool		console;  // if true print log message to stdout
    bool		classic;  // classic in stdout
    bool		colorize; // color in stdout
    bool		with_pid;
    int			zone;     // timezone offset from GMT in seconds
    int64_t		day_start;
    int64_t		day_end;
    char		day_buf[16];

    agooLogEntry		q;
    agooLogEntry		end;
    _Atomic(agooLogEntry)	head;
    _Atomic(agooLogEntry)	tail;
    atomic_flag		push_lock;
    atomic_int		wait_state;
    int			rsock;
    int			wsock;

    void		(*on_error)(agooErr err);
};

extern struct _agooLog		the_log;
extern struct _agooLogCat	fatal_cat;
extern struct _agooLogCat	error_cat;
extern struct _agooLogCat	warn_cat;
extern struct _agooLogCat	info_cat;
extern struct _agooLogCat	debug_cat;
extern struct _agooLogCat	con_cat;
extern struct _agooLogCat	req_cat;
extern struct _agooLogCat	resp_cat;
extern struct _agooLogCat	eval_cat;
extern struct _agooLogCat	push_cat;

extern void	log_init(const char *app);
extern void	open_log_file();

extern void	log_close();
extern bool	log_flush(double timeout);
extern void	log_rotate();

extern void		log_cat_reg(agooLogCat cat, const char *label, agooLogLevel level, const char *color, bool on);
extern void		log_cat_on(const char *label, bool on);
extern agooLogCat	log_cat_find(const char *label);

// Function to call to make a log entry.
extern void	log_cat(agooLogCat cat, const char *fmt, ...);
extern void	log_tid_cat(agooLogCat cat, const char *tid, const char *fmt, ...);
extern void	log_catv(agooLogCat cat, const char *tid, const char *fmt, va_list ap);

extern void	log_start(bool with_pid);

extern agooColor	find_color(const char *name);

#endif /* AGOO_LOG_H */
