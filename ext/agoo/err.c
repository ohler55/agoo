// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "err.h"

#ifdef PLATFORM_LINUX
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif

int
agoo_err_set(agooErr err, int code, const char *fmt, ...) {
    va_list	ap;

    va_start(ap, fmt);
    vsnprintf(err->msg, sizeof(err->msg), fmt, ap);
    va_end(ap);
    err->code = code;

    return err->code;
}

int
agoo_err_no(agooErr err, const char *fmt, ...) {
    int	cnt = 0;
    
    if (NULL != fmt) {
	va_list	ap;

	va_start(ap, fmt);
	cnt = vsnprintf(err->msg, sizeof(err->msg), fmt, ap);
	va_end(ap);
    }
    if (cnt < (int)sizeof(err->msg) + 2 && 0 <= cnt) {
	err->msg[cnt] = ' ';
	cnt++;
	strncpy(err->msg + cnt, strerror(errno), sizeof(err->msg) - cnt);
	err->msg[sizeof(err->msg) - 1] = '\0';
    }
    err->code = errno;

    return err->code;
}

void
agoo_err_clear(agooErr err) {
    err->code = AGOO_ERR_OK;
    *err->msg = '\0';
}

const char*
agoo_err_str(agooErrCode code) {
    const char	*str = NULL;
    
    if (code < AGOO_ERR_START) {
	str = strerror(code);
    }
    if (NULL == str) {
	switch (code) {
	case AGOO_ERR_PARSE:	str = "parse error";		break;
	case AGOO_ERR_READ:	str = "read failed";		break;
	case AGOO_ERR_WRITE:	str = "write failed";		break;
	case AGOO_ERR_ARG:	str = "invalid argument";	break;
	case AGOO_ERR_NOT_FOUND:str = "not found";		break;
	case AGOO_ERR_THREAD:	str = "thread error";		break;
	case AGOO_ERR_NETWORK:	str = "network error";		break;
	case AGOO_ERR_LOCK:	str = "lock error";		break;
	case AGOO_ERR_FREE:	str = "already freed";		break;
	case AGOO_ERR_IN_USE:	str = "in use";			break;
	case AGOO_ERR_TOO_MANY:	str = "too many";		break;
	case AGOO_ERR_TYPE:	str = "type error";		break;
	default:		str = "unknown error";		break;
	}
    }
    return str;
}
