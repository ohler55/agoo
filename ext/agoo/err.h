// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_ERR_H
#define AGOO_ERR_H

#include <errno.h>

#define AGOO_ERR_START	300
#define AGOO_ERR_INIT	{ 0, { 0 } }

#define AGOO_ERR_MEM(err, type) agoo_err_memory(err, type, __FILE__, __LINE__)

typedef enum {
    AGOO_ERR_OK	= 0,
    AGOO_ERR_MEMORY	= ENOMEM,
    AGOO_ERR_DENIED	= EACCES,
    AGOO_ERR_IMPL	= ENOSYS,
    AGOO_ERR_PARSE	= AGOO_ERR_START,
    AGOO_ERR_READ,
    AGOO_ERR_WRITE,
    AGOO_ERR_OVERFLOW,
    AGOO_ERR_ARG,
    AGOO_ERR_NOT_FOUND,
    AGOO_ERR_THREAD,
    AGOO_ERR_NETWORK,
    AGOO_ERR_LOCK,
    AGOO_ERR_FREE,
    AGOO_ERR_IN_USE,
    AGOO_ERR_TOO_MANY,
    AGOO_ERR_TYPE,
    AGOO_ERR_EVAL,
    AGOO_ERR_TLS,
    AGOO_ERR_LAST
} agooErrCode;

// The struct used to report errors or status after a function returns. The
// struct must be initialized before use as most calls that take an err
// argument will return immediately if an error has already occurred.
typedef struct _agooErr {
    int		code;
    char	msg[256];
} *agooErr;

extern int		agoo_err_set(agooErr err, int code, const char *fmt, ...);
extern int		agoo_err_no(agooErr err, const char *fmt, ...);
extern const char*	agoo_err_str(agooErrCode code);
extern void		agoo_err_clear(agooErr err);

extern int		agoo_err_memory(agooErr err, const char *type, const char *file, int line);

#endif /* AGOO_ERR_H */
