// Copyright (c) 2019, Peter Ohler, All rights reserved.

#include "kinds.h"

const char*
agoo_con_kind_str(agooConKind kind) {
    switch (kind) {
    case AGOO_CON_ANY:		return "ANY";
    case AGOO_CON_HTTP:		return "HTTP";
    case AGOO_CON_HTTPS:	return "HTTPS";
    case AGOO_CON_WS:		return "WS";
    case AGOO_CON_SSE:		return "SSE";
    default:			break;
    }
    return "UNKNOWN";
}
