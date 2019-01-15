// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gqlcobj.h"
#include "gqleval.h"

gqlCobj
gql_c_obj_create(agooErr err, gqlRef ref, gqlCclass clas) {
    gqlCobj	obj = (gqlCobj)AGOO_MALLOC(sizeof(struct _gqlCobj));
    
    if (NULL == obj) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "GraphQL C object creation failed. %s:%d", __FILE__, __LINE__);
    } else {
	obj->ref = ref;
	obj->clas = clas;
    }
    return obj;
}

gqlRef
gql_c_obj_resolve(agooErr err, gqlRef target, const char *field_name, gqlKeyVal args, gqlEvalCtx etx) {
    gqlCobj	obj = (gqlCobj)target;
    gqlCmethod	m;
    gqlCmethod	method = NULL;

    if (NULL == obj->clas) {
	agoo_err_set(err, AGOO_ERR_IMPL, "GraphQL C object has no class. %s:%d", __FILE__, __LINE__);
	return NULL;
    }
    for (m = obj->clas->methods; NULL != m->key; m++) {
	if (0 == strcmp(field_name, m->key)) {
	    method = m;
	    break;
	}
    }
    if (NULL == method) {
	agoo_err_set(err, AGOO_ERR_IMPL, "GraphQL C object has no %s field. %s:%d", field_name, __FILE__, __LINE__);
	return NULL;
    }
    return method->func(err, obj, args);
}


// TBD when a type is created, add a cobj to it

static void
eval_ctx_destroy(void *ptr) {
    // TBD free generated gqlCobj

}

int
gql_c_eval_ctx_init(agooErr err, gqlEvalCtx ctx) {
    ctx->ptr = NULL;
    ctx->destroy = eval_ctx_destroy;

    return AGOO_ERR_OK;
}

