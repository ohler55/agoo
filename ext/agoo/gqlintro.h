// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLINTRO_H
#define AGOO_GQLINTRO_H

#include "err.h"
#include "gqlcobj.h"

struct _gqlType;
struct _gqlField;

extern int	gql_intro_init(agooErr err);

extern struct _gqlCobj	gql_intro_query_root;

#endif // AGOO_GQLINTRO_H
