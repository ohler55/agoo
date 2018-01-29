// Copyright 2018 by Peter Ohler, All Rights Reserved

#ifndef __AGOO_LOG_H__
#define __AGOO_LOG_H__

#include <netdb.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <ruby.h>

#include "err.h"

typedef enum {
    FATAL	= 0,
    ERROR	= 1,
    WARN	= 2,
    INFO	= 3,
    DEBUG	= 4,
    UNKNOWN	= 5,
} LogLevel;

typedef struct _Color {
    const char	*name;
    const char	*ansi;
} *Color;

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

typedef struct _Log	*Log;

typedef struct _LogCat {
    struct _LogCat	*next;
    Log			log;
    char		label[32];
    Color		color;
    int			level;
    bool		on;
} *LogCat;

typedef struct _LogEntry {
    LogCat		cat;
    int64_t		when; // nano UTC
    char		*whatp;
    char		what[104];
    volatile bool	ready;
} *LogEntry;
	
struct _Log {
    LogCat		cats;
    char		dir[1024];
    FILE		*file;    // current output file
    int			max_files;
    int			max_size;
    long		size;     // current file size
    pthread_t		thread;
    volatile bool	done;
    bool		console;  // if true print log message to stdout
    bool		classic;  // classic in stdout
    bool		colorize; // color in stdout
    int			zone;     // timezone offset from GMT in seconds
    int64_t		day_start;
    int64_t		day_end;
    char		day_buf[16];

    LogEntry		q;
    LogEntry		end;
    _Atomic(LogEntry)	head;
    _Atomic(LogEntry)	tail;
    atomic_flag		push_lock;
    atomic_int		wait_state;
    int			rsock;
    int			wsock;
};

extern int	log_init(Err err, Log log, VALUE cfg);
extern void	log_close(Log log);
extern bool	log_flush(Log log, double timeout);

extern void	log_cat_reg(Log log, LogCat cat, const char *label, LogLevel level, const char *color, bool on);
extern void	log_cat_on(Log log, const char *label, bool on);
extern LogCat	log_cat_find(Log log, const char *label);

// Function to call to make a log entry.
extern void	log_cat(LogCat cat, const char *fmt, ...);
extern void	log_catv(LogCat cat, const char *fmt, va_list ap);

#endif /* __AGOO_LOG_H__ */
