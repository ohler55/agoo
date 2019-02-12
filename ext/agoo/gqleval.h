// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLEVAL_H
#define AGOO_GQLEVAL_H

#include <stdbool.h>

#include "err.h"

// Used for references to implemenation entities.
typedef void*	gqlRef;

typedef struct _gqlKeyVal {
    const char		*key;
    struct _gqlValue	*value;
} *gqlKeyVal;

struct _gqlDoc;
struct _gqlField;
struct _gqlSel;
struct _gqlType;
struct _gqlValue;

// Resolve field on a target to a child reference.
typedef int			(*gqlResolveFunc)(agooErr		err,
						  struct _gqlDoc	*doc,
						  gqlRef		target,
						  struct _gqlField	*field,
						  struct _gqlSel	*sel,
						  struct _gqlValue	*result,
						  int			depth);

// Determine the type of a reference. Return NULL if can't be determined.
typedef struct _gqlType*	(*gqlTypeFunc)(gqlRef ref);


extern struct _gqlValue*	gql_doc_eval(agooErr err, struct _gqlDoc *doc);
extern struct _gqlValue*	gql_get_arg_value(gqlKeyVal args, const char *key);
extern int			gql_eval_sels(agooErr err, struct _gqlDoc *doc, gqlRef ref, struct _gqlField *field, struct _gqlSel *sels, struct _gqlValue *result, int depth);
extern int			gql_set_typename(agooErr err, struct _gqlType *type, const char *key, struct _gqlValue *result);
extern struct _gqlType*		gql_root_type();

extern gqlRef			gql_root;
extern gqlResolveFunc		gql_resolve_func;
extern gqlTypeFunc		gql_type_func;
extern struct _gqlType*		_gql_root_type;
extern gqlRef			(*gql_root_op)(const char *op);

extern struct _gqlValue*	(*gql_doc_eval_func)(agooErr err, struct _gqlDoc *doc);

#endif // AGOO_GQLEVAL_H
