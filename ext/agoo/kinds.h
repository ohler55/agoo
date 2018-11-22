// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_KINDS_H
#define AGOO_KINDS_H

typedef enum {
    AGOO_CON_ANY	= '\0',
    AGOO_CON_HTTP	= 'H',
    AGOO_CON_WS		= 'W',
    AGOO_CON_SSE	= 'S',
} agooConKind;

#endif // AGOO_KINDS_H
