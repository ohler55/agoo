// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_RESPONSE_H__
#define __AGOO_RESPONSE_H__

#include <stdatomic.h>
#include <stdbool.h>

#include "server.h"
#include "text.h"

typedef struct _Header {
    struct _Header	*next;
    int			len;
    char		text[8];
} *Header;

typedef struct _Response {
    int		code;
    Header	headers;
    int		blen;
    char	*body;
} *Response;

extern int	response_len(Response res);
extern void	response_fill(Response res, char *buf);

#endif // __AGOO_RESPONSE_H__
