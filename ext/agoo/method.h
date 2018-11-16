// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_METHOD_H
#define AGOO_METHOD_H

typedef enum {
    AGOO_CONNECT	= 'C',
    AGOO_DELETE		= 'D',
    AGOO_GET		= 'G',
    AGOO_HEAD		= 'H',
    AGOO_OPTIONS	= 'O',
    AGOO_POST		= 'P',
    AGOO_PUT		= 'U',
    AGOO_PATCH		= 'T',
    AGOO_ALL		= 'A',
    AGOO_NONE		= '\0',

    AGOO_ON_MSG		= 'M', // use for on_message callback
    AGOO_ON_BIN		= 'B', // use for on_message callback with binary (ASCII8BIT)
    AGOO_ON_CLOSE	= 'X', // use for on_close callback
    AGOO_ON_SHUTDOWN	= 'S', // use for on_shotdown callback
    AGOO_ON_EMPTY	= 'E', // use for on_drained callback
    AGOO_ON_ERROR	= 'F', // use for on_error callback
} agooMethod;

#endif // AGOO_METHOD_H
