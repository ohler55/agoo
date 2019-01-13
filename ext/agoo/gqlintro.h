// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLINTRO_H
#define AGOO_GQLINTRO_H

#include "err.h"
#include "gqlcobj.h"

struct _gqlType;

extern int	gql_intro_init(agooErr err);

extern struct _gqlCobj		gql_intro_query_root;
extern gqlCobj			gql_type_intro_create(agooErr err, struct _gqlType *type);


#endif // AGOO_GQLINTRO_H
