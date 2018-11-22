// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_HTTP_H
#define AGOO_HTTP_H

#include <stdbool.h>

#include "err.h"

extern void		agoo_http_init();
extern void		agoo_http_cleanup();

extern int		agoo_http_header_ok(agooErr err, const char *key, int klen, const char *value, int vlen);

extern const char*	agoo_http_code_message(int code);

#endif // AGOO_HTTP_H
