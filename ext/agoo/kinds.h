// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_KINDS_H
#define AGOO_KINDS_H

typedef enum {
    AGOO_CON_ANY	= '\0',
    AGOO_CON_HTTP	= 'H',
    AGOO_CON_HTTPS	= 'T',
    AGOO_CON_WS		= 'W',
    AGOO_CON_SSE	= 'S',
} agooConKind;

extern const char*	agoo_con_kind_str(agooConKind kind);

#endif // AGOO_KINDS_H
