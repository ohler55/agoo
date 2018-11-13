// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_ERR_H
#define AGOO_ERR_H

#include <errno.h>

#define ERR_START	300
#define ERR_INIT	{ 0, { 0 } }

typedef enum {
    ERR_OK	= 0,
    ERR_MEMORY	= ENOMEM,
    ERR_DENIED	= EACCES,
    ERR_IMPL	= ENOSYS,
    ERR_PARSE	= ERR_START,
    ERR_READ,
    ERR_WRITE,
    ERR_OVERFLOW,
    ERR_ARG,
    ERR_NOT_FOUND,
    ERR_THREAD,
    ERR_NETWORK,
    ERR_LOCK,
    ERR_FREE,
    ERR_IN_USE,
    ERR_TOO_MANY,
    ERR_TYPE,
    ERR_LAST
} ErrCode;

// The struct used to report errors or status after a function returns. The
// struct must be initialized before use as most calls that take an err
// argument will return immediately if an error has already occurred.
typedef struct _Err {
    int		code;
    char	msg[256];
} *Err;

extern int		err_set(Err err, int code, const char *fmt, ...);
extern int		err_no(Err err, const char *fmt, ...);
extern const char*	err_str(ErrCode code);
extern void		err_clear(Err err);

#endif /* AGOO_ERR_H */
