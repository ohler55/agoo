// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLJSON_H
#define AGOO_GQLJSON_H


#include <stdlib.h>

#include "err.h"

struct _gqlValue;

extern struct _gqlValue*	gql_json_parse(agooErr err, const char *json, size_t len);

#endif // AGOO_GQLJSON_H
