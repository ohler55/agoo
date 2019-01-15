// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLCOBJ_H
#define AGOO_GQLCOBJ_H

#include "gqleval.h"

struct _gqlCobj;
struct _gqlEvalCtx;

typedef struct _gqlCmethod {
    const char		*key;
    gqlRef		(*func)(agooErr err, struct _gqlCobj *obj, gqlKeyVal args);
    
} *gqlCmethod;

typedef struct _gqlCclass {
    const char		*name;
    gqlCmethod		methods; // TBD use a hash instead of a simple array
} *gqlCclass;

typedef struct _gqlCobj {
    gqlCclass		clas;
    gqlRef		ref;
} *gqlCobj;


extern gqlCobj	gql_c_obj_create(agooErr err, gqlRef ref, gqlCclass clas);
extern gqlRef	gql_c_obj_resolve(agooErr err, gqlRef target, const char *field_name, gqlKeyVal args, gqlEvalCtx etx);
extern int	gql_c_eval_ctx_init(agooErr err, struct _gqlEvalCtx *ctx);


#endif // AGOO_GQLCOBJ_H
