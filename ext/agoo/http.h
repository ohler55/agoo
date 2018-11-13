// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_HTTP_H
#define AGOO_HTTP_H

#include <stdbool.h>

#include "err.h"

extern void		http_init();
extern void		http_cleanup();

extern int		http_header_ok(Err err, const char *key, int klen, const char *value, int vlen);

extern const char*	http_code_message(int code);

#endif // AGOO_HTTP_H
