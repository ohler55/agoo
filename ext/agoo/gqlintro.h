// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLINTRO_H
#define AGOO_GQLINTRO_H

#include "err.h"
#include "gqlcobj.h"

struct _gqlField;
struct _gqlSel;
struct _gqlType;
struct _gqlValue;

extern int	gql_intro_init(agooErr err);

extern int	gql_intro_eval(agooErr err, struct _gqlDoc *doc, struct _gqlSel *sel, struct _gqlValue *result, int depth);

#endif // AGOO_GQLINTRO_H
