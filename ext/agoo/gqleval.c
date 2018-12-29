// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "gqleval.h"
#include "graphql.h"
#include "gqlvalue.h"
#include "http.h"
#include "req.h"
#include "res.h"
#include "sdl.h"

#define MAX_RESOLVE_ARGS	16

gqlRef		gql_root = NULL;
gqlResolveFunc	gql_resolve_func = NULL;
gqlCoerceFunc	gql_coerce_func = NULL;
gqlValue	(*gql_doc_eval_func)(agooErr err, gqlDoc doc) = NULL;

// TBD errors should have message, location, and path
static void
err_resp(agooRes res, agooErr err, int status) {
    char	buf[256];
    int		cnt;

    cnt = snprintf(buf, sizeof(buf),
		   "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n{\"errors\":[{\"message\":\"%s\"}]}",
		   status, agoo_http_code_message(status), strlen(err->msg) + 27, err->msg);

    agoo_res_set_message(res, agoo_text_create(buf, cnt));
}

static void
value_resp(agooRes res, gqlValue result, int status, int indent) {
    char		buf[256];
    struct _agooErr	err = AGOO_ERR_INIT;
    int			cnt;
    agooText		text = agoo_text_allocate(4094);
    gqlValue		msg = gql_object_create(&err);

    if (NULL == msg) {
	// TBD error
    }
    if (AGOO_ERR_OK != gql_object_set(&err, msg, "data", result)) {
	err_resp(res, &err, 500);
	return;
    }
    text = gql_value_json(text, msg, indent, 0);

    cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n",
		   status, agoo_http_code_message(status), text->len);
    text = agoo_text_prepend(text, buf, cnt);
    agoo_res_set_message(res, text);
}

static int
eval_sels(agooErr err, gqlDoc doc, gqlRef ref, gqlSel sels, gqlValue result) {
    gqlSel	sel;

    for (sel = sels; NULL != sel; sel = sel->next) {
	if (NULL != sel->inline_frag) {
	    // TBD
	} else if (NULL != sel->frag) {
	    // TBD
	} else { // TBD handle __xxx
	    struct _gqlKeyVal	args[MAX_RESOLVE_ARGS];
	    gqlKeyVal		a;
	    gqlValue		child;
	    gqlRef		r2;
	    const char		*key = sel->name;
	    gqlSelArg		sa;
	    int			i;

	    for (sa = sel->args, a = args, i = 0; NULL != sa; sa = sa->next, a++, i++) {
		if (MAX_RESOLVE_ARGS <= i) {
		    return agoo_err_set(err, AGOO_ERR_EVAL, "Too many arguments to %s.", sel->name);
		}
		a->key = sa->name;
		// TBD handle variables as well
		a->value = sa->value;
	    }
	    a->key = NULL;
	    if (NULL == (r2 = gql_resolve_func(err, ref, sel->name, args))) {
		return err->code;
	    }
	    if (NULL != sel->alias) {
		key = sel->alias;
	    }
	    if (NULL == sel->sels) {
		if (NULL == (child = gql_coerce_func(err, r2, sel->type)) ||
		    AGOO_ERR_OK != gql_object_set(err, result, key, child)) {
		    return err->code;
		}
	    } else {
		if (NULL == (child = gql_object_create(err)) ||
		    AGOO_ERR_OK != gql_object_set(err, result, key, child)) {
		    return err->code;
		}
		if (AGOO_ERR_OK != eval_sels(err, doc, r2, sel->sels, child)) {
		    return err->code;
		}		    
	    }
	}
    }
    // TBD call resolve
    // TBD if sel->sels then recurse else coerce

	    // TBD check for __ or other, use differet root for each

	    // TBD start building result
	    //   call recursive func

    return AGOO_ERR_OK;
}

gqlValue
gql_doc_eval(agooErr err, gqlDoc doc) {
    gqlValue	result;

    if (NULL == doc->op) {
	agoo_err_set(err, AGOO_ERR_EVAL, "Failed to identify operation in doc.");
	return NULL;
    }
    if (NULL != (result = gql_object_create(err))) {
	gqlRef		op_root;
	const char	*key;
	
	switch (doc->op->kind) {
	case GQL_QUERY:
	    key = "query";
	    break;
	case GQL_MUTATION:
	    key = "mutation";
	    break;
	case GQL_SUBSCRIPTION:
	    key = "subscription";
	    break;
	default:
	    agoo_err_set(err, AGOO_ERR_EVAL, "Not a valid operation on the root object.");
	    return NULL;
	    break;
	}
	if (NULL == (op_root = gql_resolve_func(err, gql_root, key, NULL))) {
	    agoo_err_set(err, AGOO_ERR_EVAL, "root %s is not supported.", key);
	    return NULL;
	}
	if (AGOO_ERR_OK != eval_sels(err, doc, op_root, doc->op->sels, result)) {
	    gql_value_destroy(result);
	    return NULL;
	}
    }
    return result;
}

void
gql_eval_get_hook(agooReq req) {
    struct _agooErr	err = AGOO_ERR_INIT;
    const char		*gq; // graphql query
    const char		*op_name = NULL;
    const char		*var_json = NULL;
    int			qlen;
    int			indent = 0;
    gqlDoc		doc;
    gqlValue		result;
    
    printf("*** GET hook called - query: %s\n", req->query.start);

    if (NULL != (gq = agoo_req_query_value(req, "indent", 6, &qlen))) {
	indent = (int)strtol(gq, NULL, 10);
    }
    if (NULL == (gq = agoo_req_query_value(req, "query", 5, &qlen))) {
	err_resp(req->res, &err, 500);
	return;
    }
    qlen = agoo_req_query_decode((char*)gq, qlen);
    if (NULL == (doc = sdl_parse_doc(&err, gq, qlen))) {
	err_resp(req->res, &err, 500);
	return;
    }
    if (NULL != (op_name = agoo_req_query_value(req, "operationName", 13, &qlen))) {
	gqlOp	op;

	// This null terminates the string.
	agoo_req_query_decode((char*)op_name, qlen);
	for (op = doc->ops; NULL != op; op = op->next) {
	    if (NULL != op->name && 0 == strcmp(op_name, op->name)) {
		doc->op = op;
		break;
	    }
	}
    } else {
	doc->op = doc->ops;
    }
    if (NULL != (var_json = agoo_req_query_value(req, "variables", 9, &qlen))) {
	agoo_req_query_decode((char*)var_json, qlen);
	if (NULL != doc->op) {
	    // TBD parse into doc->op->vars
	}
    }
    if (NULL == gql_doc_eval_func) {
	result = gql_doc_eval(&err, doc);
    } else {
	result = gql_doc_eval_func(&err, doc);
    }
    if (NULL == result) {
	err_resp(req->res, &err, 500);
	return;
    }
    value_resp(req->res, result, 200, indent);
}

void
gql_eval_post_hook(agooReq req) {
    
    printf("*** POST hook called\n");

    // TBD detect introspection
    //  start resolving by callout to some global handler as needed
    //   pass target, field, args
    //   return json or gqlValue
    // for handler, if introspection then handler here else global
}
