// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_RESPONSE_H
#define AGOO_RESPONSE_H

#include <stdbool.h>

#include "atomic.h"
#include "server.h"
#include "text.h"

typedef struct _agooHeader {
    struct _agooHeader	*next;
    int			len;
    char		text[8];
} *agooHeader;

typedef struct _agooResponse {
    int		code;
    agooHeader	headers;
    int		blen;
    char	*body;
} *agooResponse;

extern int	agoo_response_len(agooResponse res);
extern void	agoo_response_fill(agooResponse res, char *buf);

#endif // AGOO_RESPONSE_H
