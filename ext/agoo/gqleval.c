// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "gqleval.h"
#include "gqlintro.h"
#include "gqljson.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "http.h"
#include "log.h"
#include "req.h"
#include "res.h"
#include "sdl.h"
#include "sectime.h"
#include "text.h"

#define MAX_RESOLVE_ARGS	16

gqlRef		gql_root = NULL;
gqlType		_gql_root_type = NULL;
gqlResolveFunc	gql_resolve_func = NULL;
gqlTypeFunc	gql_type_func = NULL;
gqlRef		(*gql_root_op)(const char *op) = NULL;

static const char	graphql_content_type[] = "application/graphql";
static const char	indent_str[] = "indent";
static const char	json_content_type[] = "application/json";
static const char	operation_name_str[] = "operationName";
static const char	query_str[] = "query";
static const char	variables_str[] = "variables";


gqlValue	(*gql_doc_eval_func)(agooErr err, gqlDoc doc) = NULL;

// TBD errors should have message, location, and path
static void
err_resp(agooRes res, agooErr err, int status) {
    char		buf[256];
    int			cnt;
    int64_t		now = agoo_now_nano();
    time_t		t = (time_t)(now / 1000000000LL);
    long long		frac = (long long)now % 1000000000LL;
    struct _agooTime	at;
    const char		*code = agoo_err_str(err->code);
    int			clen = strlen(code);

    agoo_sectime(t, &at);
    cnt = snprintf(buf, sizeof(buf),
		   "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n{\"errors\":[{\"message\":\"%s\",\"code\":\"%s\",\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d.%09lldZ\"}]}",
		   status, agoo_http_code_message(status), strlen(err->msg) + 27 + 45 + clen + 10, err->msg,
		   code,
		   at.year, at.mon, at.day, at.hour, at.min, at.sec, frac);

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
	AGOO_ERR_MEM(&err, "response");
	err_resp(res, &err, 500);
	gql_value_destroy(result);
	return;
    }
    if (AGOO_ERR_OK != gql_object_set(&err, msg, "data", result)) {
	err_resp(res, &err, 500);
	gql_value_destroy(result);
	return;
    }
    text = gql_value_json(text, msg, indent, 0);
    gql_value_destroy(msg); // also destroys result

    cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n",
		   status, agoo_http_code_message(status), text->len);
    if (NULL == (text = agoo_text_prepend(text, buf, cnt))) {
	agoo_log_cat(&agoo_error_cat, "Failed to allocate memory for a response.");
    }
    agoo_res_set_message(res, text);
}

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
frag_include(gqlDoc doc, gqlFrag frag, gqlRef ref) {
    gqlDirUse	dir;

    if (NULL != frag->on) {
	if (frag->on != doc->funcs.type(ref)) {
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

int
gql_set_typename(agooErr err, gqlType type, const char *key, gqlValue result) {
    gqlValue	child;

    if (NULL == type) {
	return agoo_err_set(err, AGOO_ERR_EVAL, "Internal error, failed to determine the __typename.");
    }
    if (NULL == (child = gql_string_create(err, type->name, -1)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, child)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

int
gql_eval_sels(agooErr err, gqlDoc doc, gqlRef ref, gqlField field, gqlSel sels, gqlValue result, int depth) {
    gqlSel	sel;
    gqlField	sf = NULL;

    // TBD if depth over max then return an error

    for (sel = sels; NULL != sel; sel = sel->next) {
	if (NULL != field) {
	    if (NULL == sel->name) {
		sf = field;
	    } else {
		sf = gql_type_get_field(field->type, sel->name);
	    }
	} else {
	    sf = NULL;
	}
	if (NULL != sel->inline_frag) {
	    if (frag_include(doc, sel->inline_frag, ref)) {
		if (AGOO_ERR_OK != gql_eval_sels(err, doc, ref, sf, sel->inline_frag->sels, result, depth)) {
		    return err->code;
		}
	    }
	} else if (NULL != sel->frag) {
	    gqlFrag	frag;

	    for (frag = doc->frags; NULL != frag; frag = frag->next) {
		if (NULL != frag->name && 0 == strcmp(frag->name, sel->frag)) {
		    if (frag_include(doc, frag, ref)) {
			if (AGOO_ERR_OK != gql_eval_sels(err, doc, ref, sf, frag->sels, result, depth)) {
			    return err->code;
			}
		    }
		}
	    }
	} else {
	    if (AGOO_ERR_OK != doc->funcs.resolve(err, doc, ref, sf, sel, result, depth)) {
		return err->code;
	    }
	}
    }
    return AGOO_ERR_OK;
}

gqlType
gql_root_type() {
    if (NULL == _gql_root_type && NULL != gql_type_func) {
	_gql_root_type = gql_type_func(gql_root);
    }
    return _gql_root_type;
}

gqlValue
gql_doc_eval(agooErr err, gqlDoc doc) {
    gqlValue	result;

    if (NULL == doc->op) {
	agoo_err_set(err, AGOO_ERR_EVAL, "Failed to identify operation in doc.");
	return NULL;
    }
    if (NULL != (result = gql_object_create(err))) {
	const char	*key;
	gqlField	field = NULL;
	struct _gqlSel	sel;

	memset(&sel, 0, sizeof(sel));
	doc->funcs.resolve = gql_resolve_func;
	doc->funcs.type = gql_type_func;

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
	    gql_value_destroy(result);
	    return NULL;
	    break;
	}
	if (NULL == _gql_root_type && NULL != gql_type_func) {
	    _gql_root_type = gql_type_func(gql_root);
	}
	if (NULL != _gql_root_type) {
	    field = gql_type_get_field(_gql_root_type, key);
	}
	if (NULL == field) {
	    agoo_err_set(err, AGOO_ERR_EVAL, "GraphQL not initialized.");
	    gql_value_destroy(result);
	    return NULL;
	}
	sel.name = key;
	sel.type = _gql_root_type;
	sel.dir = doc->op->dir;
	sel.sels = doc->op->sels;
	if (AGOO_ERR_OK != doc->funcs.resolve(err, doc, gql_root, field, &sel, result, 0)) {
	    gql_value_destroy(result);
	    return NULL;
	}
    }
    return result;
}

static gqlVar
parse_query_vars(agooErr err, const char *var_json, int vlen) {
    gqlValue	vlist = NULL;
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

	link->value = NULL;
	if (NULL == v) {
	    goto DONE;
	}
	v->next = vars;
	vars = v;
    }
DONE:
    if (NULL != vlist) {
	gql_value_destroy(vlist);
    }
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
    gql_doc_destroy(doc);
    if (NULL == result) {
	err_resp(req->res, &err, 500);
	return;
    }
    value_resp(req->res, result, 200, indent);
}

static gqlValue
eval_post(agooErr err, agooReq req) {
    gqlDoc		doc = NULL;
    const char		*op_name = NULL;
    const char		*var_json = NULL;
    const char		*query = NULL;
    int			oplen;
    int			vlen;
    int			qlen = 0;
    gqlVar		vars = NULL;
    const char		*s;
    int			len;
    gqlValue		result = NULL;
    gqlValue		j = NULL;

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
	gqlLink	m;

	if (NULL != (j = gql_json_parse(err, req->body.start, req->body.len))) {
	    if (GQL_SCALAR_OBJECT != j->type->scalar_kind) {
		agoo_err_set(err, AGOO_ERR_TYPE, "JSON request must be an object");
		goto DONE;
	    }
	    for (m = j->members; NULL != m; m = m->next) {
		if (0 == strcmp("query", m->key)) {
		    if (NULL == (s = gql_string_get(m->value))) {
			agoo_err_set(err, AGOO_ERR_TYPE, "query must be an string");
			goto DONE;
		    }
		    query = s;
		    qlen = strlen(s);
		} else if (0 == strcmp("operationName", m->key)) {
		    if (NULL == (s = gql_string_get(m->value))) {
			agoo_err_set(err, AGOO_ERR_TYPE, "operationName must be an string");
			goto DONE;
		    }
		    op_name = s;
		    oplen = strlen(s);
		} else if (0 == strcmp("variables", m->key)) {
		    gqlLink	link;

		    if (GQL_SCALAR_OBJECT != m->value->type->scalar_kind) {
			agoo_err_set(err, AGOO_ERR_EVAL, "expected variables to be an object.");
			goto DONE;
		    }
		    for (link = m->value->members; NULL != link; link = link->next) {
			gqlVar	v = gql_op_var_create(err, link->key, link->value->type, link->value);

			link->value = NULL; // TBD is this correct?
			if (NULL == v) {
			    goto DONE;
			}
			v->next = vars;
			vars = v;
		    }
		}
	    }
	    if (NULL == (doc = sdl_parse_doc(err, query, qlen, vars))) {
		goto DONE;
	    }
	} else {
	    goto DONE;
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

DONE:
    gql_doc_destroy(doc);
    gql_value_destroy(j);

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

gqlValue
gql_get_arg_value(gqlKeyVal args, const char *key) {
    gqlValue	value = NULL;

    if (NULL != args) {
	for (; NULL != args->key; args++) {
	    if (0 == strcmp(key, args->key)) {
		value = args->value;
		break;
	    }
	}
    }
    return value;
}
