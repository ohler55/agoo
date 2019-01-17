// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLCOBJ_H
#define AGOO_GQLCOBJ_H

#include "gqleval.h"

struct _gqlCobj;

typedef struct _gqlCmethod {
    const char		*key;
    int			(*func)(agooErr err, struct _gqlDoc *doc, struct _gqlCobj *obj, struct _gqlField *field, struct _gqlSel *sel, struct _gqlValue *result, int depth);
} *gqlCmethod;

typedef struct _gqlCclass {
    const char		*name;
    gqlCmethod		methods; // TBD use a hash instead of a simple array
} *gqlCclass;

typedef struct _gqlCobj {
    gqlCclass		clas;
    void		*ptr;
} *gqlCobj;

#endif // AGOO_GQLCOBJ_H
