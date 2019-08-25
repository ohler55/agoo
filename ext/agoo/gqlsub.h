// Copyright (c) 2019, Peter Ohler, All rights reserved.

#ifndef AGOO_GQL_SUB_H
#define AGOO_GQL_SUB_H

#include <stdbool.h>
#include <stdlib.h>

#include "err.h"

struct _agooCon;
struct _gqlDoc;

typedef struct _gqlSub {
    struct _gqlSub	*next;
    struct _agooCon	*con;
    char		*subject;
    struct _gqlDoc	*query;
} *gqlSub;

extern gqlSub	gql_sub_create(agooErr err, struct _agooCon *con, const char *subject, struct _gqlDoc *query);
extern void	gql_sub_destroy(gqlSub sub);

#endif // AGOO_GQL_SUB_H
