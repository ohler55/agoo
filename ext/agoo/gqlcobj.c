// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gqlcobj.h"
#include "gqleval.h"
#include "gqlvalue.h"
#include "graphql.h"


gqlType
gql_cobj_ref_type(gqlRef ref) {
    gqlCobj	obj = (gqlCobj)ref;

    if (NULL != obj && NULL != obj->clas) {
	return gql_type_get(obj->clas->name);
    }
    return NULL;
}

int
gql_cobj_resolve(agooErr err, gqlDoc doc, gqlRef target, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlCobj	obj = (gqlCobj)target;
    gqlCmethod	method;

    for (method = obj->clas->methods; NULL != method->key; method++) {
	if (0 == strcmp(method->key, sel->name)) {
	    return method->func(err, doc, obj, field, sel, result, depth);
	}
    }
    return agoo_err_set(err, AGOO_ERR_EVAL, "%s is not a field on %s.", sel->name, obj->clas->name);
}

// TBD when a type is created, add a cobj to it
