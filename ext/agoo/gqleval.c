// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "gqleval.h"
#include "gqljson.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "http.h"
#include "log.h"
#include "req.h"
#include "res.h"
#include "sdl.h"

#define MAX_RESOLVE_ARGS	16

typedef struct _implFuncs {
    gqlResolveFunc	resolve;
    gqlCoerceFunc	coerce;
    gqlTypeFunc		type;
    gqlIterateFunc	iterate;
} *ImplFuncs;

gqlRef		gql_root = NULL;
gqlResolveFunc	gql_resolve_func = NULL;
gqlCoerceFunc	gql_coerce_func = NULL;
gqlTypeFunc	gql_type_func = NULL;
gqlIterateFunc	gql_iterate_func = NULL;

static const char	graphql_content_type[] = "application/graphql";
static const char	indent_str[] = "indent";
static const char	json_content_type[] = "application/json";
static const char	operation_name_str[] = "operationName";
static const char	query_str[] = "query";
static const char	variables_str[] = "variables";

gqlValue	(*gql_doc_eval_func)(agooErr err, gqlDoc doc) = NULL;

static int	eval_sels(agooErr err, gqlDoc doc, gqlRef ref, gqlSel sels, gqlValue result, ImplFuncs funcs);
static int	eval_sel(agooErr err, gqlDoc doc, gqlRef ref, gqlSel sel, gqlValue result, ImplFuncs funcs);

// TBD errors should have message, location, and path
static void
err_resp(agooRes res, agooErr err, int status) {
    char	buf[256];
    int		cnt;
    int64_t	now = agoo_now_nano();
    time_t	t = (time_t)(now / 1000000000LL);
    long long	frac = (long long)now % 1000000000LL;
    struct tm	*tm = gmtime(&t);
    const char	*code = agoo_err_str(err->code);
    int		clen = strlen(code);

    cnt = snprintf(buf, sizeof(buf),
		   "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n{\"errors\":[{\"message\":\"%s\",\"code\":\"%s\",\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d.%09lldZ\"}]}",
		   status, agoo_http_code_message(status), strlen(err->msg) + 27 + 45 + clen + 10, err->msg,
		   code,
		   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, frac);

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
	agoo_err_set(&err, AGOO_ERR_MEMORY, "Out of memory.");
	err_resp(res, &err, 500);
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
    gql_value_destroy(result);
}

static gqlRef
resolve_intro(agooErr err, gqlRef target, const char *field_name, gqlKeyVal args) {

    // TBD
    
    return NULL;
}

static gqlValue
coerce_intro(agooErr err, gqlRef ref, struct _gqlType *type) {
    return (gqlValue)ref;
}

static gqlType
type_intro(gqlRef ref) {

    // TBD 
    
    return NULL;
}

static int
iterate_intro(agooErr err, gqlRef ref, int (*cb)(agooErr err, gqlRef ref, void *ctx), void *ctx) {

    // TBD
    
    return AGOO_ERR_OK;
}

static struct _implFuncs	intro_funcs = {
    .resolve = resolve_intro,
    .coerce = coerce_intro,
    .type = type_intro,
    .iterate = iterate_intro,
};

gqlValue
doc_var_value(gqlDoc doc, const char *key) {
    gqlVar	var;

    // look in doc->vars and doc->op->vars
    if (NULL != doc->op) {
	for (var = doc->op->vars; NULL != var; var = var->next) {
	    if (0 == strcmp(key, var->name)) {
		return var->value;
	    }
	}
    }
    for (var = doc->vars; NULL != var; var = var->next) {
	if (0 == strcmp(key, var->name)) {
	    return var->value;
	}
    }
    return NULL;
}

static bool
frag_include(gqlDoc doc, gqlFrag frag, gqlRef ref, ImplFuncs funcs) {
    gqlDirUse	dir;

    if (NULL != frag->on) {
	if (frag->on != funcs->type(ref)) {
	    return false;
	}
    }
    for (dir = frag->dir; NULL != dir; dir = dir->next) {
	if (NULL != dir->dir && 0 == strcmp("skip", dir->dir->name)) {
	    gqlLink	arg;

	    for (arg = dir->args; NULL != arg; arg = arg->next) {
		if (0 == strcmp("if", arg->key) && NULL != arg->value) {
		    if (&gql_bool_type == arg->value->type && arg->value->b) {
			return false;
		    } else {
			const char	*key = NULL;

			if (&gql_var_type == arg->value->type) {
			    if (arg->value->str.alloced) {
				key = arg->value->str.ptr;
			    } else {
				key = arg->value->str.a;
			    }
			}
			if (NULL != key) {
			    gqlValue	var = doc_var_value(doc, key);

			    if (NULL != var && &gql_bool_type == var->type && var->b) {
				return false;
			    }
			}
		    }
		}
	    }
	}
	if (NULL != dir->dir && 0 == strcmp("include", dir->dir->name)) {
	    gqlLink	arg;

	    for (arg = dir->args; NULL != arg; arg = arg->next) {
		if (0 == strcmp("if", arg->key) && NULL != arg->value) {
		    if (&gql_bool_type == arg->value->type && !arg->value->b) {
			return false;
		    } else {
			const char	*key = NULL;

			if (&gql_var_type == arg->value->type) {
			    if (arg->value->str.alloced) {
				key = arg->value->str.ptr;
			    } else {
				key = arg->value->str.a;
			    }
			}
			if (NULL != key) {
			    gqlValue	var = doc_var_value(doc, key);

			    if (NULL != var && &gql_bool_type == var->type && !var->b) {
				return false;
			    }
			}
		    }
		}
	    }
	}
    }
    return true;
}

typedef struct _evalCtx {
    gqlDoc	doc;
    gqlRef	ref;
    gqlType	base;
    gqlSel	sels;
    gqlValue	result;
    ImplFuncs	funcs;
} *EvalCtx;

static int
eval_list_sel(agooErr err, gqlRef ref, void *ctx) {
    EvalCtx	ec = (EvalCtx)ctx;
    gqlValue	obj;
    
    if (NULL == ec->sels) {
	if (NULL == (obj = ec->funcs->coerce(err, ref, ec->base)) ||
	    AGOO_ERR_OK != gql_list_append(err, ec->result, obj)) {
	    return err->code;
	}
    } else {
	if (NULL == (obj = gql_object_create(err)) ||
	    AGOO_ERR_OK != eval_sels(err, ec->doc, ref, ec->sels, obj, ec->funcs) ||
	    AGOO_ERR_OK != gql_list_append(err, ec->result, obj)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static int
eval_sel(agooErr err, gqlDoc doc, gqlRef ref, gqlSel sel, gqlValue result, ImplFuncs funcs) {
    if (NULL != sel->inline_frag) {
	if (frag_include(doc, sel->inline_frag, ref, funcs)) {
	    if (AGOO_ERR_OK != eval_sels(err, doc, ref, sel->inline_frag->sels, result, funcs)) {
		return err->code;
	    }
	}
    } else if (NULL != sel->frag) {
	gqlFrag	frag;

	for (frag = doc->frags; NULL != frag; frag = frag->next) {
	    if (NULL != frag->name && 0 == strcmp(frag->name, sel->frag)) {
		if (frag_include(doc, frag, ref, funcs)) {
		    if (AGOO_ERR_OK != eval_sels(err, doc, ref, frag->sels, result, funcs)) {
			return err->code;
		    }
		}
	    }
	}
    } else {
	struct _gqlKeyVal	args[MAX_RESOLVE_ARGS];
	gqlKeyVal		a;
	gqlValue		child;
	gqlRef			r2;
	const char		*key = sel->name;
	gqlSelArg		sa;
	int			i;

	for (sa = sel->args, a = args, i = 0; NULL != sa; sa = sa->next, a++, i++) {
	    if (MAX_RESOLVE_ARGS <= i) {
		return agoo_err_set(err, AGOO_ERR_EVAL, "Too many arguments to %s.", sel->name);
	    }
	    a->key = sa->name;
	    if (NULL != sa->var) {
		a->value = sa->var->value;
	    } else {
		a->value = sa->value;
	    }
	    // TBD compare value type with expected type
	    //   how to get expected type?
	}
	a->key = NULL;
	if (NULL != sel->alias) {
	    key = sel->alias;
	}
	// TBD __type nd __schema from the query root only, ___typename from anywhere
	if ('_' == *sel->name && '_' == sel->name[1]) {
	    funcs = &intro_funcs;
	}
	if (NULL == (r2 = funcs->resolve(err, ref, sel->name, args))) {
	    return err->code;
	}
	if (NULL != sel->type && GQL_LIST == sel->type->kind) { // TBD only for lists of objects, not scalars
	    struct _evalCtx	ec = {
		.doc = doc,
		.ref = ref,
		.base = sel->type->base,
		.sels = sel->sels,
		.result = gql_list_create(err, NULL),
		.funcs = funcs,
	    };

	    if (NULL == ec.result ||
		AGOO_ERR_OK != funcs->iterate(err, r2, eval_list_sel, &ec) ||
		AGOO_ERR_OK != gql_object_set(err, result, key, ec.result)) {
		return err->code;
	    }
	} else if (NULL == sel->sels) {
	    if (NULL == (child = funcs->coerce(err, r2, sel->type)) ||
		AGOO_ERR_OK != gql_object_set(err, result, key, child)) {
		return err->code;
	    }
	} else {
	    if (NULL == (child = gql_object_create(err)) ||
		AGOO_ERR_OK != gql_object_set(err, result, key, child)) {
		return err->code;
	    }
	    if (AGOO_ERR_OK != eval_sels(err, doc, r2, sel->sels, child, funcs)) {
		return err->code;
	    }
	}
    }
    return AGOO_ERR_OK;
}

static int
eval_sels(agooErr err, gqlDoc doc, gqlRef ref, gqlSel sels, gqlValue result, ImplFuncs funcs) {
    gqlSel	sel;

    for (sel = sels; NULL != sel; sel = sel->next) {
	if (AGOO_ERR_OK != eval_sel(err, doc, ref, sel, result, funcs)) {
	    return err->code;
	}
    }
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
	struct _implFuncs	funcs = {
	    .resolve = gql_resolve_func,
	    .coerce = gql_coerce_func,
	    .type = gql_type_func,
	    .iterate = gql_iterate_func,
	};
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
	// TBD pass in type, get type from 'schema' type at key
	if (AGOO_ERR_OK != eval_sels(err, doc, op_root, doc->op->sels, result, &funcs)) {
	    gql_value_destroy(result);
	    return NULL;
	}
    }
    return result;
}

static gqlVar
parse_query_vars(agooErr err, const char *var_json, int vlen) {
    gqlValue	vlist;
    gqlLink	link;
    gqlVar	vars = NULL;
    
    vlen = agoo_req_query_decode((char*)var_json, vlen);
    if (NULL == (vlist = gql_json_parse(err, var_json, vlen))) {
	goto DONE;
    }
    if (GQL_SCALAR_OBJECT != vlist->type->scalar_kind) {
	agoo_err_set(err, AGOO_ERR_EVAL, "expected variables to be an object.");
	goto DONE;
    }
    for (link = vlist->members; NULL != link; link = link->next) {
	gqlVar	v = gql_op_var_create(err, link->key, link->value->type, link->value);

	if (NULL == v) {
	    goto DONE;
	}
	v->next = vars;
	vars = v;
    }
DONE:
    return vars;
}

static void
set_doc_op(gqlDoc doc, const char *op_name, int oplen) {
    if (NULL != op_name) {
	gqlOp	op;

	// This null terminates the string.
	agoo_req_query_decode((char*)op_name, oplen);
	for (op = doc->ops; NULL != op; op = op->next) {
	    if (NULL != op->name && 0 == strcmp(op_name, op->name)) {
		doc->op = op;
		break;
	    }
	}
    } else {
	doc->op = doc->ops;
    }
}

void
gql_eval_get_hook(agooReq req) {
    struct _agooErr	err = AGOO_ERR_INIT;
    const char		*gq; // graphql query
    const char		*op_name = NULL;
    const char		*var_json = NULL;
    int			qlen;
    int			oplen;
    int			vlen;
    int			indent = 0;
    gqlDoc		doc;
    gqlValue		result;
    gqlVar		vars = NULL;
    
    if (NULL != (gq = agoo_req_query_value(req, indent_str, sizeof(indent_str) - 1, &qlen))) {
	indent = (int)strtol(gq, NULL, 10);
    }
    if (NULL == (gq = agoo_req_query_value(req, query_str, sizeof(query_str) - 1, &qlen))) {
	err_resp(req->res, &err, 500);
	return;
    }
    op_name = agoo_req_query_value(req, operation_name_str, sizeof(operation_name_str) - 1, &oplen);
    var_json = agoo_req_query_value(req, variables_str, sizeof(variables_str) - 1, &vlen);

    if (NULL != var_json) {
	if (NULL == (vars = parse_query_vars(&err, var_json, vlen)) && AGOO_ERR_OK != err.code) {
	    err_resp(req->res, &err, 400);
	    return;
	}
    }
    // Only call after extracting the variables as it terminates the string with a \0.
    qlen = agoo_req_query_decode((char*)gq, qlen);
    if (NULL == (doc = sdl_parse_doc(&err, gq, qlen, vars))) {
	err_resp(req->res, &err, 500);
	return;
    }
    set_doc_op(doc, op_name, oplen);

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

static gqlValue
eval_post(agooErr err, agooReq req) {
    gqlDoc		doc;
    const char		*op_name = NULL;
    const char		*var_json = NULL;
    const char		*query = NULL;
    int			oplen;
    int			vlen;
    int			qlen = 0;
    gqlVar		vars = NULL;
    const char		*s;
    int			len;
    gqlValue		result;

    // TBD handle query parameter and concatenate with body query if present

    op_name = agoo_req_query_value(req, operation_name_str, sizeof(operation_name_str) - 1, &oplen);
    var_json = agoo_req_query_value(req, variables_str, sizeof(variables_str) - 1, &vlen);

    if (NULL != var_json) {
	if (NULL == (vars = parse_query_vars(err, var_json, vlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
    }
    if (NULL == (s = agoo_req_header_value(req, "Content-Type", &len))) {
	agoo_err_set(err, AGOO_ERR_TYPE, "required Content-Type not in the HTTP header");
	return NULL;
    }
    if (0 == strncmp(graphql_content_type, s, sizeof(graphql_content_type) - 1)) {
	if (NULL == (doc = sdl_parse_doc(err, req->body.start, req->body.len, vars))) {
	    return NULL;
	}
    } else if (0 == strncmp(json_content_type, s, sizeof(json_content_type) - 1)) {
	gqlValue	j;
	gqlLink		m;
	
	if (NULL != (j = gql_json_parse(err, req->body.start, req->body.len))) {
	    if (GQL_SCALAR_OBJECT != j->type->scalar_kind) {
		agoo_err_set(err, AGOO_ERR_TYPE, "JSON request must be an object");
		return NULL;
	    }
	    for (m = j->members; NULL != m; m = m->next) {
		if (0 == strcmp("query", m->key)) {
		    if (NULL == (s = gql_string_get(m->value))) {
			agoo_err_set(err, AGOO_ERR_TYPE, "query must be an string");
			return NULL;
		    }
		    query = s;
		    qlen = strlen(s);
		} else if (0 == strcmp("operationName", m->key)) {
		    if (NULL == (s = gql_string_get(m->value))) {
			agoo_err_set(err, AGOO_ERR_TYPE, "operationName must be an string");
			return NULL;
		    }
		    op_name = s;
		    oplen = strlen(s);
		} else if (0 == strcmp("variables", m->key)) {
		    gqlLink	link;

		    if (GQL_SCALAR_OBJECT != m->value->type->scalar_kind) {
			agoo_err_set(err, AGOO_ERR_EVAL, "expected variables to be an object.");
			return NULL;
		    }
		    for (link = m->value->members; NULL != link; link = link->next) {
			gqlVar	v = gql_op_var_create(err, link->key, link->value->type, link->value);

			if (NULL == v) {
			    return NULL;
			}
			v->next = vars;
			vars = v;
		    }
		}
	    }
	    if (NULL == (doc = sdl_parse_doc(err, query, qlen, vars))) {
		return NULL;
	    }
	} else {
	    return NULL;
	}
    } else {
	agoo_err_set(err, AGOO_ERR_TYPE, "unsupported content type");
	return NULL;
    }
    set_doc_op(doc, op_name, oplen);

    if (NULL == gql_doc_eval_func) {
	result = gql_doc_eval(err, doc);
    } else {
	result = gql_doc_eval_func(err, doc);
    }
    return result;
}

void
gql_eval_post_hook(agooReq req) {
    struct _agooErr	err = AGOO_ERR_INIT;
    gqlValue		result;
    const char		*s;
    int			len;
    int			indent = 0;

    if (NULL != (s = agoo_req_query_value(req, indent_str, sizeof(indent_str) - 1, &len))) {
	indent = (int)strtol(s, NULL, 10);
    }

    if (NULL == (result = eval_post(&err, req))) {
	err_resp(req->res, &err, 400);
    } else {
	value_resp(req->res, result, 200, indent);
    }
}
