// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_HTTP_H__
#define __AGOO_HTTP_H__

#include <stdbool.h>

extern void		http_init();
extern void		http_cleanup();

extern void		http_header_ok(const char *key, int klen, const char *value, int vlen);

extern const char*	http_code_message(int code);

#endif // __AGOO_HTTP_H__
