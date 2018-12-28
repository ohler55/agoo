// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLEVAL_H
#define AGOO_GQLEVAL_H

#include "err.h"

// Used for references to implemenation entities.
typedef void*	gqlRef;

typedef struct _gqlKeyVal {
    const char		*key;
    struct _gqlValue	*value;
} *gqlKeyVal;

struct _gqlDoc;
struct _gqlType;
struct _gqlValue;

// Resolve field on a target to a child reference.
typedef gqlRef			(*gqlResolveFunc)(agooErr err, gqlRef target, const char *field_name, gqlKeyVal args);

// Coerce an implemenation reference into a gqlValue.
typedef struct _gqlValue*	(*gqlCoerceFunc)(agooErr err, gqlRef ref, struct _gqlType *type);

extern struct _gqlValue*	gql_doc_eval(agooErr err, struct _gqlDoc *doc);

extern gqlRef			gql_root;
extern gqlResolveFunc		gql_resolve_func;
extern gqlCoerceFunc		gql_coerce_func;
extern struct _gqlValue*	(*gql_doc_eval_func)(agooErr err, struct _gqlDoc *doc);

#endif // AGOO_GQLEVAL_H
