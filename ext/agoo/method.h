// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_METHOD_H
#define AGOO_METHOD_H

typedef enum {
    CONNECT	= 'C',
    DELETE	= 'D',
    GET		= 'G',
    HEAD	= 'H',
    OPTIONS	= 'O',
    POST	= 'P',
    PUT		= 'U',
    PATCH	= 'T',
    ALL		= 'A',
    NONE	= '\0',

    ON_MSG	= 'M', // use for on_message callback
    ON_BIN	= 'B', // use for on_message callback with binary (ASCII8BIT)
    ON_CLOSE	= 'X', // use for on_close callback
    ON_SHUTDOWN	= 'S', // use for on_shotdown callback
    ON_EMPTY	= 'E', // use for on_drained callback
    ON_ERROR	= 'F', // use for on_error callback
} Method;

#endif // AGOO_METHOD_H
