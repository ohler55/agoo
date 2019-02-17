// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLCOBJ_H
#define AGOO_GQLCOBJ_H

#include "gqleval.h"

struct _gqlCobj;
struct _gqlDoc;
struct _gqlField;
struct _gqlSel;
struct _gqlType;
struct _gqlValue;

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

extern struct _gqlType*	gql_cobj_ref_type(gqlRef ref);
extern int		gql_cobj_resolve(agooErr		err,
					 struct _gqlDoc		*doc,
					 gqlRef			target,
					 struct _gqlField	*field,
					 struct _gqlSel		*sel,
					 struct _gqlValue	*result,
					 int			depth);

#endif // AGOO_GQLCOBJ_H
