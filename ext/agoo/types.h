// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_TYPES_H__
#define __AGOO_TYPES_H__

typedef enum {
    CONNECT	= 'C',
    DELETE	= 'D',
    GET		= 'G',
    HEAD	= 'H',
    OPTIONS	= 'O',
    POST	= 'P',
    PUT		= 'U',
    ALL		= 'A',
    NONE	= '\0',

    ON_MSG	= 'M', // use for on_message callback
    ON_CLOSE	= 'X', // use for on_close callback
    ON_SHUTDOWN	= 'S', // use for on_shotdown callback
    ON_EMPTY	= 'E', // use for on_drained callback
} Method;

typedef enum {
    CON_HTTP	= 'H',
    CON_WS	= 'W',
    CON_SSE	= 'S',
} ConKind;

#endif // __AGOO_TYPES_H__
