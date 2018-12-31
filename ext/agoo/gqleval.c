// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "gqleval.h"
#include "graphql.h"
#include "gqlvalue.h"
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
		if (0 == strcmp("if", arg->key) && NULL != arg->value && &gql_bool_type == arg->value->type && arg->value->b) {
		    return false;
		}
	    }
	}
	if (NULL != dir->dir && 0 == strcmp("include", dir->dir->name)) {
	    gqlLink	arg;

	    for (arg = dir->args; NULL != arg; arg = arg->next) {
		if (0 == strcmp("if", arg->key) && NULL != arg->value && &gql_bool_type == arg->value->type && !arg->value->b) {
		    return false;
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
	}
	a->key = NULL;
	if (NULL != sel->alias) {
	    key = sel->alias;
	}
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
	if (AGOO_ERR_OK != eval_sels(err, doc, op_root, doc->op->sels, result, &funcs)) {
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
	    // TBD parse JSON or SDL into doc->op->vars
	    //  if starts with a " then JSON else SDL
	    //    maybe the same except for the "
	    //      lists and objects ok?
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
