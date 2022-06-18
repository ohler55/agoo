// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdint.h>
#include <stdlib.h>

#include <ruby.h>
#include <ruby/thread.h>

#include "debug.h"
#include "err.h"
#include "gqleval.h"
#include "gqlintro.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "pub.h"
#include "request.h"
#include "sdl.h"
#include "server.h"

typedef struct _eval {
    gqlDoc	doc;
    agooErr	err;
    gqlValue	value;
} *Eval;

typedef struct _typeClass {
    gqlType	type;
    const char	*classname;
} *TypeClass;

typedef struct _bhArgs {
    agooErr	err;
    agooReq	req;
    agooText	headers;
} *bhArgs;

static VALUE		graphql_class = Qundef;
static VALUE		vroot = Qnil;
static VALUE		build_headers_func = Qnil;
static VALUE		arg_clas = Qnil;
static VALUE		field_clas = Qnil;
static VALUE		type_clas = Qnil;

static ID		call_id;

static TypeClass	type_class_map = NULL;

static int
make_ruby_use(agooErr    err,
	      VALUE      root,
	      const char *method,
	      const char *type_name,
	      bool       fresh,
	      gqlType    schema_type,
	      const char *desc) {
    gqlType		type;
    gqlDirUse		use;
    volatile VALUE	v;
    ID			m = rb_intern(method);

    if (!rb_respond_to(root, m)) {
	return AGOO_ERR_OK;
    }
    switch (rb_obj_method_arity(root, m)) {
    case 0:
	v = rb_funcall(root, m, 0);
	break;
    case 1:
	v = rb_funcall(root, m, 1, Qnil);
	break;
    case 2:
	v = rb_funcall(root, m, 2, Qnil, Qnil);
	break;
    default:
	v = Qnil;
	break;
    }
    if (Qnil == v) {
	return AGOO_ERR_OK;
    }
    if (NULL == (type = gql_type_get(type_name))) {
	return agoo_err_set(err, AGOO_ERR_ARG, "Failed to find the '%s' type.", type_name);
    }
    if (fresh) {
	if (NULL == gql_type_field(err, schema_type, method, type, NULL, desc, 0)) {
	    return err->code;
	}
    }
    if (!gql_type_has_directive_use(type, "ruby")) {
	if (NULL == (use = gql_dir_use_create(err, "ruby")) ||
	    AGOO_ERR_OK != gql_dir_use_arg(err, use, "class", gql_string_create(err, rb_obj_classname(v), 0)) ||
	    AGOO_ERR_OK != gql_type_directive_use(err, type, use)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static VALUE
rescue_error(VALUE x, VALUE ignore) {
    Eval		eval = (Eval)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);

    agoo_err_set(eval->err, AGOO_ERR_EVAL, "%s: %s", classname, ms);
    if (rb_respond_to(info, rb_intern("code"))) {
	VALUE	code = rb_funcall(info, rb_intern("code"), 0);

	if (RUBY_T_FIXNUM == rb_type(code)) {
	    eval->err->code = -FIX2INT(code);
	}
    }
    return Qfalse;
}

static VALUE
call_eval(VALUE x) {
    Eval	eval = (Eval)(void*)x;

    eval->value = gql_doc_eval(eval->err, eval->doc);

    return Qnil;
}

static void*
protect_eval(void *x) {
    rb_rescue2(call_eval, (VALUE)x, rescue_error, (VALUE)x, rb_eException, (VALUE)0);

    return NULL;
}

// Hidden ruby function.
extern int ruby_thread_has_gvl_p();

static gqlValue
eval_wrap(agooErr err, gqlDoc doc) {
    struct _eval	eval = {
	.doc = doc,
	.err = err,
	.value = NULL,
    };
    if (ruby_thread_has_gvl_p()) {
	protect_eval(&eval);
    } else {
	rb_thread_call_with_gvl(protect_eval, &eval);
    }
    return eval.value;
}

static VALUE
gval_to_ruby(gqlValue value) {
    volatile VALUE	rval = Qnil;

    if (NULL == value || NULL == value->type) {
	return Qnil;
    }
    if (GQL_SCALAR == value->type->kind) {
	switch (value->type->scalar_kind) {
	case GQL_SCALAR_BOOL:
	    rval = value->b ? Qtrue : Qfalse;
	    break;
	case GQL_SCALAR_INT:
	    rval = RB_INT2NUM(value->i);
	    break;
	case GQL_SCALAR_I64:
	    rval = RB_LONG2NUM(value->i64);
	    break;
	case GQL_SCALAR_FLOAT:
	    rval = DBL2NUM(value->f);
	    break;
	case GQL_SCALAR_STRING:
	case GQL_SCALAR_TOKEN:
	case GQL_SCALAR_ID:
	    rval = rb_str_new_cstr(gql_string_get(value));
	    break;
	case GQL_SCALAR_TIME: {
	    time_t	secs = (time_t)(value->time / 1000000000LL);

	    rval = rb_time_nano_new(secs, (long)(value->time - secs * 1000000000LL));
	    break;
	}
	case GQL_SCALAR_UUID: {
	    char	buf[64];

	     sprintf(buf, "%08lx-%04lx-%04lx-%04lx-%012lx",
		     (unsigned long)(value->uuid.hi >> 32),
		     (unsigned long)((value->uuid.hi >> 16) & 0x000000000000FFFFUL),
		     (unsigned long)(value->uuid.hi & 0x000000000000FFFFUL),
		     (unsigned long)(value->uuid.lo >> 48),
		     (unsigned long)(value->uuid.lo & 0x0000FFFFFFFFFFFFUL));
	    rval = rb_str_new_cstr(buf);
	    break;
	}
	case GQL_SCALAR_LIST: {
	    gqlLink	link;

	    rval = rb_ary_new();
	    for (link = value->members; NULL != link; link = link->next) {
		rb_ary_push(rval, gval_to_ruby(link->value));
	    }
	    break;
	}
	case GQL_SCALAR_OBJECT: {
	    gqlLink	link;

	    rval = rb_hash_new();
	    for (link = value->members; NULL != link; link = link->next) {
		rb_hash_aset(rval, rb_str_new_cstr(link->key), gval_to_ruby(link->value));

	    }
	    break;
	}
	default:
	    break;
	}
    } else if (GQL_ENUM == value->type->kind) {
	rval = rb_str_new_cstr(gql_string_get(value));
    }
    return rval;
}

static VALUE
ref_to_string(gqlRef ref) {
    volatile VALUE	value;

    if (T_STRING == rb_type((VALUE)ref)) {
	value = (VALUE)ref;
    } else {
	value = rb_funcall((VALUE)ref, rb_intern("to_s"), 0);
    }
    return value;
}

static gqlValue
time_to_time(agooErr err, VALUE rval) {
    long long	sec;
    long long	nsec;

#ifdef HAVE_RB_TIME_TIMESPEC
    // rb_time_timespec as well as rb_time_timeeval have a bug that causes an
    // exception to be raised if a time is before 1970 on 32 bit systems so
    // check the timespec size and use the ruby calls if a 32 bit system.
    if (16 <= sizeof(struct timespec)) {
	struct timespec	ts = rb_time_timespec(rval);

	sec = (long long)ts.tv_sec;
	nsec = ts.tv_nsec;
    } else {
	sec = rb_num2ll(rb_funcall2(rval, oj_tv_sec_id, 0, 0));
	nsec = rb_num2ll(rb_funcall2(rval, oj_tv_nsec_id, 0, 0));
    }
#else
    sec = rb_num2ll(rb_funcall2(rval, rb_intern("tv_sec"), 0, 0));
    nsec = rb_num2ll(rb_funcall2(rval, rb_intern("tv_nsec"), 0, 0));
#endif
    return gql_time_create(err, (int64_t)(sec * 1000000000LL + nsec));
}

static gqlValue
coerce(agooErr err, gqlRef ref, gqlType type) {
    gqlValue		value = NULL;
    volatile VALUE	v;

    if (Qnil == (VALUE)ref) {
	return gql_null_create(err);
    }
    if (NULL != type && GQL_NON_NULL == type->kind) {
	type = type->base;
    }
    if (NULL == type) {
	// This is really an error but make a best effort anyway.
	switch (rb_type((VALUE)ref)) {
	case RUBY_T_FLOAT:
	    value = gql_float_create(err, rb_num2dbl(rb_to_float((VALUE)ref)));
	    break;
	case RUBY_T_STRING:
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), (int)RSTRING_LEN(v));
	    break;
	case RUBY_T_ARRAY: {
	    gqlValue	v;
	    VALUE	a = (VALUE)ref;
	    int		cnt = (int)RARRAY_LEN(a);
	    int		i;

	    value = gql_list_create(err, NULL);
	    for (i = 0; i < cnt; i++) {
		if (NULL == (v = coerce(err, (gqlRef)rb_ary_entry(a, i), type->base)) ||
		    AGOO_ERR_OK != gql_list_append(err, value, v)) {
		    return NULL;
		}
	    }
	    break;
	}
	case RUBY_T_TRUE:
	    value = gql_bool_create(err, true);
	    break;
	case RUBY_T_FALSE:
	    value = gql_bool_create(err, false);
	    break;
	case RUBY_T_SYMBOL:
	    value = gql_string_create(err, rb_id2name(rb_sym2id((VALUE)ref)), -1);
	    break;
	case RUBY_T_FIXNUM: {
	    long	i = RB_NUM2LONG(rb_to_int((VALUE)ref));

	    if (i < INT32_MIN || INT32_MAX < i) {
		value = gql_i64_create(err, i);
	    } else {
		value = gql_int_create(err, (int)i);
	    }
	    break;
	}
	case RUBY_T_NIL:
	    value = gql_null_create(err);
	    break;
	case RUBY_T_OBJECT: {
	    VALUE	clas = rb_obj_class((VALUE)ref);

	    if (rb_cTime == clas) {
		value = time_to_time(err, (VALUE)ref);
	    }
	    break;
	}
	default:
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), (int)RSTRING_LEN(v));
	    break;
	}
    } else if (GQL_SCALAR == type->kind) {
	switch (type->scalar_kind) {
	case GQL_SCALAR_BOOL:
	    if (Qtrue == (VALUE)ref) {
		value = gql_bool_create(err, true);
	    } else if (Qfalse == (VALUE)ref) {
		value = gql_bool_create(err, false);
	    }
	    break;
	case GQL_SCALAR_INT:
	    value = gql_int_create(err, RB_NUM2INT(rb_to_int((VALUE)ref)));
	    break;
	case GQL_SCALAR_I64:
	    value = gql_i64_create(err, RB_NUM2LONG(rb_to_int((VALUE)ref)));
	    break;
	case GQL_SCALAR_FLOAT:
	    value = gql_float_create(err, rb_num2dbl(rb_to_float((VALUE)ref)));
	    break;
	case GQL_SCALAR_STRING:
	case GQL_SCALAR_TOKEN:
	case GQL_SCALAR_ID:
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), (int)RSTRING_LEN(v));
	    break;
	case GQL_SCALAR_TIME: {
	    VALUE	clas = rb_obj_class((VALUE)ref);

	    if (rb_cTime == clas) {
		value = time_to_time(err, (VALUE)ref);
	    }
	    break;
	}
	case GQL_SCALAR_UUID:
	    v = ref_to_string(ref);
	    value = gql_uuid_str_create(err, rb_string_value_ptr(&v), (int)RSTRING_LEN(v));
	    break;
	case GQL_SCALAR_LIST: {
	    gqlValue	v;
	    VALUE	a = (VALUE)ref;
	    int		cnt = (int)RARRAY_LEN(a);
	    int		i;

	    value = gql_list_create(err, NULL);
	    for (i = 0; i < cnt; i++) {

		if (NULL == (v =coerce(err, (gqlRef)rb_ary_entry(a, i), type->base)) ||
		    AGOO_ERR_OK != gql_list_append(err, value, v)) {
		    return NULL;
		}
	    }
	    break;
	}
	default:
	    break;
	}
	if (NULL == value) {
	    if (AGOO_ERR_OK == err->code) {
		agoo_err_set(err, AGOO_ERR_EVAL, "Failed to coerce a %s into a %s.", rb_obj_classname((VALUE)ref), type->name);
	    }
	}
    } else if (GQL_ENUM == type->kind) {
	v = ref_to_string(ref);
	value = gql_string_create(err, rb_string_value_ptr(&v), (int)RSTRING_LEN(v));
    } else {
	rb_raise(rb_eStandardError, "Can not coerce a non-scalar into a %s.", type->name);
    }
    return value;
}

static gqlType
ref_type(gqlRef ref) {
    gqlType	type = NULL;

    if (NULL != type_class_map) {
	TypeClass	tc;
	const char	*classname = rb_obj_classname((VALUE)ref);

	for (tc = type_class_map; NULL != tc->type; tc++) {
	    if (0 == strcmp(classname, tc->classname)) {
		type = tc->type;
		break;
	    }
	}
    }
    return type;
}

static VALUE
make_plan(gqlSel sel) {

    // TBD create a plan object that wraps the sel, create children as needed
    // plan is not valid outside of method
    // should be able to create SDL from it

    return Qnil;
}

static int
resolve(agooErr err, gqlDoc doc, gqlRef target, gqlField field, gqlSel sel, gqlValue result, int depth) {
    volatile VALUE	child;
    volatile VALUE	rreq = Qnil;
    VALUE		obj = (VALUE)target;
    int			d2 = depth + 1;
    const char		*key = sel->name;
    ID			method;
    int			arity;

    if ('_' == *sel->name && '_' == sel->name[1]) {
	if (0 == strcmp("__typename", sel->name)) {
	    if (AGOO_ERR_OK != gql_set_typename(err, ref_type(target), key, result)) {
		return err->code;
	    }
	    return AGOO_ERR_OK;
	}
	switch (doc->op->kind) {
	case GQL_QUERY:
	    return gql_intro_eval(err, doc, sel, result, depth);
	case GQL_MUTATION:
	    return agoo_err_set(err, AGOO_ERR_EVAL, "%s can not be called on a mutation.", sel->name);
	case GQL_SUBSCRIPTION:
	    return agoo_err_set(err, AGOO_ERR_EVAL, "%s can not be called on a subscription.", sel->name);
	default:
	    return agoo_err_set(err, AGOO_ERR_EVAL, "Not a valid operation on the root object.");
	}
    }
    method = rb_intern(sel->name);
    arity = rb_obj_method_arity(obj, method);
    if (0 == arity) {
	child = rb_funcall(obj, method, 0);
    } else {
	volatile VALUE	rargs = rb_hash_new();
	gqlSelArg	sa;
	gqlValue	v;

	for (sa = sel->args; NULL != sa; sa = sa->next) {
	    if (NULL != sa->var) {
		v = sa->var->value;
	    } else {
		v = sa->value;
	    }
	    if (NULL == v) {
		v = gql_null_create(err);
	    }
	    if (NULL != field) {
		gqlArg	fa;

		for (fa = field->args; NULL != fa; fa = fa->next) {
		    if (0 == strcmp(sa->name, fa->name)) {
			if (v->type != fa->type && GQL_SCALAR_VAR != v->type->scalar_kind) {
			    if (AGOO_ERR_OK != gql_value_convert(err, v, fa->type)) {
				return err->code;
			    }
			}
			break;
		    }
		}
	    }
	    rb_hash_aset(rargs, rb_str_new_cstr(sa->name), gval_to_ruby(v));
	}
	if (-1 == arity || 1 == arity) {
	    child = rb_funcall(obj, method, 1, rargs);
	} else {
	    if (NULL == doc->ctx) {
		rreq = request_wrap(doc->req);
		doc->ctx = (void*)rreq;
	    } else {
		rreq = (VALUE)doc->ctx;
	    }
	    if (-2 == arity || 2 == arity) {
		child = rb_funcall(obj, method, 2, rargs, rreq);
	    } else {
		child = rb_funcall(obj, method, 3, rargs, rreq, make_plan(sel));
	    }
	}
    }
    if (GQL_SUBSCRIPTION == doc->op->kind && RUBY_T_STRING == rb_type(child)) {
	gqlValue	c;

	if (NULL == (c = gql_string_create(err, rb_string_value_ptr(&child), (int)RSTRING_LEN(child))) ||
	    AGOO_ERR_OK != gql_object_set(err, result, "subject", c)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL != sel->type && GQL_LIST == sel->type->kind) {
	gqlValue	list;
	int		cnt;
	int		i;

	rb_check_type(child, RUBY_T_ARRAY);
	if (NULL == (list = gql_list_create(err, NULL))) {
	    return err->code;
	}
	cnt = (int)RARRAY_LEN(child);
	for (i = 0; i < cnt; i++) {
	    gqlValue	co;

	    if (NULL != sel->type->base && GQL_SCALAR != sel->type->base->kind) {
		struct _gqlField	cf;

		if (NULL == (co = gql_object_create(err)) ||
		    AGOO_ERR_OK != gql_list_append(err, list, co)) {
		    return err->code;
		}
		memset(&cf, 0, sizeof(cf));
		cf.type = sel->type->base;

		if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)rb_ary_entry(child, i), &cf, sel->sels, co, d2)) {
		    return err->code;
		}
	    } else {
		if (NULL == (co = coerce(err, (gqlRef)rb_ary_entry(child, i), sel->type->base)) ||
		    AGOO_ERR_OK != gql_list_append(err, list, co)) {
		    return err->code;
		}
	    }
	}
	if (AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	    return err->code;
	}
    } else if (NULL == sel->sels) {
	gqlValue	cv;

	if (NULL == (cv = coerce(err, (gqlRef)child, sel->type)) ||
	    AGOO_ERR_OK != gql_object_set(err, result, key, cv)) {
	    return err->code;
	}
    } else {
	gqlValue	co;

	if (0 == depth) {
	    co = result;
	} else if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	    return err->code;
	}
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)child, field, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static void
ruby_types_cb(gqlType type, void *ctx) {
    gqlDirUse	dir;

    for (dir = type->dir; NULL != dir; dir = dir->next) {
	if (NULL != dir->dir && 0 == strcmp("ruby", dir->dir->name)) {
	    if (NULL != type_class_map) {
		TypeClass	tc = &type_class_map[*(int*)ctx];
		gqlLink		arg;

		tc->type = type;
		for (arg = dir->args; NULL != arg; arg = arg->next) {
		    if (0 == strcmp("class", arg->key)) {
			tc->classname = gql_string_get(arg->value);
		    }
		}
	    }
	    *((int*)ctx) += 1;
	}
    }
}

static int
build_type_class_map(agooErr err) {
    int		cnt = 0;

    AGOO_FREE(type_class_map);
    type_class_map = NULL;

    gql_type_iterate(ruby_types_cb, &cnt);

    if (NULL == (type_class_map = (TypeClass)AGOO_CALLOC(cnt + 1, sizeof(struct _typeClass)))) {
	return AGOO_ERR_MEM(err, "GraphQL Class map");
    }
    cnt = 0;
    gql_type_iterate(ruby_types_cb, &cnt);

    return AGOO_ERR_OK;
}

static gqlRef
root_op(const char *op) {
    return (gqlRef)rb_funcall((VALUE)gql_root, rb_intern(op), 0);
}

static VALUE
rescue_yield_error(VALUE x, VALUE ignore) {
    agooErr		err = (agooErr)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);

    agoo_err_set(err, AGOO_ERR_EVAL, "%s: %s", classname, ms);

    return Qfalse;
}

static VALUE
rescue_yield(VALUE x) {
    rb_yield_values2(0, NULL);

    return Qnil;
}

/* Document-method: schema
 *
 * call-seq: schema(root) { }
 *
 * Start the GraphQL/Ruby integration by assigning a root Ruby object to the
 * GraphQL environment. Within the block passed to the function SDL strings or
 * files can be loaded. On exiting the block validation of the loaded schema
 * is performed.
 *
 * Note that the _@ruby_ directive is added to the _schema_ type as well as
 * the _Query_, _Mutation_, and _Subscription_ types based on the _root_
 * class. Any _@ruby_ directives on the object types in the SDL will also
 * associate a GraphQL and Ruby class. The association will be used to
 * validate the Ruby class as a way to verify the class implements the methods
 * described by the GraphQL type. The association is also use for resolving
 * @skip and @include directives.
 */
static VALUE
graphql_schema(VALUE self, VALUE root) {
    struct _agooErr	err = AGOO_ERR_INIT;
    gqlDir		dir;
    gqlDirUse		use;
    gqlType		schema_type;
    bool		fresh = false;
    gqlType		string_nn;

    if (!rb_block_given_p()) {
	rb_raise(rb_eStandardError, "A block is required.");
    }
    if (AGOO_ERR_OK != gql_init(&err)) {
	printf("*-*-* %s\n", err.msg);
	exit(1);
    }
    if (NULL == (dir = gql_directive_create(&err, "ruby", "Associates a Ruby class with a GraphQL type.", 0))) {
	printf("*-*-* %s\n", err.msg);
	exit(2);
    }
    if (NULL == (string_nn = gql_assure_nonnull(&err, &gql_string_type)) ||
	NULL == gql_dir_arg(&err, dir, "class", string_nn, NULL, 0, NULL)) {
	printf("*-*-* %s\n", err.msg);
	exit(3);
    }
    if (AGOO_ERR_OK != gql_directive_on(&err, dir, "SCHEMA", 6) ||
	AGOO_ERR_OK != gql_directive_on(&err, dir, "OBJECT", 6)) {
	printf("*-*-* %s\n", err.msg);
	exit(4);
    }
    gql_root = (gqlRef)root;
    vroot = root;
    rb_gc_register_address(&vroot);

    gql_doc_eval_func = eval_wrap;
    gql_resolve_func = resolve;
    gql_type_func = ref_type;
    gql_root_op = root_op;

    if (NULL == (use = gql_dir_use_create(&err, "ruby"))) {
	printf("*-*-* %s\n", err.msg);
	exit(5);
    }
    if (AGOO_ERR_OK != gql_dir_use_arg(&err, use, "class", gql_string_create(&err, rb_obj_classname(root), 0))) {
	printf("*-*-* %s\n", err.msg);
	exit(6);
    }
    rb_rescue2(rescue_yield, Qnil, rescue_yield_error, (VALUE)&err, rb_eException, (VALUE)0);
    if (AGOO_ERR_OK != err.code) {
	printf("*-*-* %s\n", err.msg);
	exit(7);
    }
    if (NULL == (schema_type = gql_type_get("schema"))) {
	if (NULL == (schema_type = gql_schema_create(&err, "The GraphQL root Object.", 0))) {
	    printf("*-*-* %s\n", err.msg);
	    exit(8);
	}
	fresh = true;
    }

    if (AGOO_ERR_OK != gql_type_directive_use(&err, schema_type, use) ||
	AGOO_ERR_OK != make_ruby_use(&err, root, "query", "Query", fresh, schema_type, "Root level query.") ||
	AGOO_ERR_OK != make_ruby_use(&err, root, "mutation", "Mutation", fresh, schema_type, "Root level mutation.") ||
	AGOO_ERR_OK != make_ruby_use(&err, root, "subscription", "Subscription", fresh, schema_type, "Root level subscription.")) {
	printf("*-*-* %s\n", err.msg);
	exit(9);
    }
    if (AGOO_ERR_OK != gql_validate(&err) ||
	AGOO_ERR_OK != build_type_class_map(&err)) {
	printf("*-*-* %s\n", err.msg);
	exit(10);
    }
    return Qnil;
}

/* Document-method: load
 *
 * call-seq: load(sdl)
 *
 * Load an SDL string. This should only be called in a block provided to a
 * call to _#schema_.
 */
static VALUE
graphql_load(VALUE self, VALUE sdl) {
    struct _agooErr	err = AGOO_ERR_INIT;

    if (NULL == gql_root) {
	rb_raise(rb_eStandardError, "GraphQL root not set. Use Agoo::GraphQL.schema.");
    }
    rb_check_type(sdl, T_STRING);
    if (AGOO_ERR_OK != sdl_parse(&err, StringValuePtr(sdl), (int)RSTRING_LEN(sdl))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    return Qnil;
}

/* Document-method: load_file
 *
 * call-seq: load_file(sdl_file)
 *
 * Load an SDL file. This should only be called in a block provided to a call
 * to _#schema_.
 */
static VALUE
graphql_load_file(VALUE self, VALUE path) {
    struct _agooErr	err = AGOO_ERR_INIT;
    FILE		*f;
    long		len;
    char		*sdl;

    if (NULL == gql_root) {
	rb_raise(rb_eStandardError, "GraphQL root not set. Use Agoo::GraphQL.schema.");
    }
    rb_check_type(path, T_STRING);
    if (NULL == (f = fopen(StringValuePtr(path), "r"))) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    }
    if (0 != fseek(f, 0, SEEK_END)) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    }
    if (0 > (len = ftell(f))) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    }
    sdl = ALLOC_N(char, len + 1);
    if (0 != fseek(f, 0, SEEK_SET)) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    }
    if (len != (long)fread(sdl, 1, len, f)) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    } else {
	sdl[len] = '\0';
    }
    fclose(f);
    if (AGOO_ERR_OK != sdl_parse(&err, sdl, (int)len)) {
	xfree(sdl);
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    xfree(sdl);

    return Qnil;
}

/* Document-method: sdl_dump
 *
 * call-seq: sdl_dump()
 *
 * The preferred method of inspecting a GraphQL schema is to use introspection
 * queries but for debugging and for reviewing the schema a dump of the schema
 * as SDL can be helpful. The _#sdl_dump_ method returns the schema as an SDL
 * string.
 *
 * - *options* [_Hash_] options
 *
 *   - *:with_description* [_true_|_false_] if true the description strings are included. If false they are not included.
 *
 *   - *:all* [_true_|_false_] if true all types and directives are included in the dump. If false only the user created types are include.
 *
 */
static VALUE
graphql_sdl_dump(VALUE self, VALUE options) {
    agooText		t = agoo_text_allocate(4096);
    volatile VALUE	dump;
    VALUE		v;
    bool		with_desc = true;
    bool		all = false;

    Check_Type(options, T_HASH);

    v = rb_hash_aref(options, ID2SYM(rb_intern("with_descriptions")));
    if (Qnil != v) {
	with_desc = (Qtrue == v);
    }
    v = rb_hash_aref(options, ID2SYM(rb_intern("all")));
    if (Qnil != v) {
	all = (Qtrue == v);
    }
    if (NULL == (t = gql_schema_sdl(t, with_desc, all))) {
	rb_raise(rb_eNoMemError, "Failed to allocate memory for a schema dump.");
    }
    dump = rb_str_new(t->text, t->len);
    agoo_text_release(t);

    return dump;
}

static void
type_cb(gqlType type, void *ctx) {
    VALUE	rtypes = (VALUE)ctx;
    VALUE	t;

    if (GQL_OBJECT != type->kind || type->core) {
	return;
    }
    t = rb_obj_alloc(type_clas);
    rb_ivar_set(t, rb_intern("@name"), rb_str_new_cstr(type->name));
    if (NULL != type->desc) {
	rb_ivar_set(t, rb_intern("@description"), rb_str_new_cstr(type->desc));
    }
    if (NULL != type->fields) {
	VALUE		fields = rb_ary_new();
	VALUE		field;
	gqlField	f;

	rb_ivar_set(t, rb_intern("@fields"), fields);
	for (f = type->fields; NULL != f; f = f->next) {
	    field = rb_obj_alloc(field_clas);
	    rb_ary_push(fields, field);
	    rb_ivar_set(field, rb_intern("@name"), rb_str_new_cstr(f->name));
	    if (NULL != f->desc) {
		rb_ivar_set(field, rb_intern("@description"), rb_str_new_cstr(f->desc));
	    }
	    if (NULL != f->type) {
		rb_ivar_set(field, rb_intern("@type_name"), rb_str_new_cstr(f->type->name));
	    }
	    if (NULL != f->default_value) {
		rb_ivar_set(field, rb_intern("@default_value"), gval_to_ruby(f->default_value));
	    }
	    if (NULL != f->args) {
		VALUE	args = rb_ary_new();
		VALUE	arg;
		gqlArg	a;

		rb_ivar_set(field, rb_intern("@args"), args);
		for (a = f->args; NULL != a; a = a->next) {
		    arg = rb_obj_alloc(arg_clas);
		    rb_ary_push(args, arg);
		    rb_ivar_set(arg, rb_intern("@name"), rb_str_new_cstr(a->name));
		    if (NULL != a->desc) {
			rb_ivar_set(arg, rb_intern("@description"), rb_str_new_cstr(a->desc));
		    }
		    if (NULL != a->type) {
			rb_ivar_set(arg, rb_intern("@type_name"), rb_str_new_cstr(a->type->name));
		    }
		    if (NULL != a->default_value) {
			rb_ivar_set(arg, rb_intern("@default_value"), gval_to_ruby(a->default_value));
		    }
		}
	    }
	}
    }
    rb_ary_push(rtypes, t);
}

/* Document-method: sdl_types
 *
 * call-seq: sdl_types()
 *
 * Returns an array of all SDL types as Ruby objects.
 */
static VALUE
graphql_sdl_types(VALUE self) {
    VALUE	rtypes = rb_ary_new();

    gql_type_iterate(type_cb, (void*)rtypes);

    return rtypes;
}

/* Document-method: publish
 *
 * call-seq: publish(subject, event)
 *
 * Publish an event on the given subject. A subject must be a String while the
 * event must be one of the objects represented by the the GraphQL
 * schema. Generally the subjects are selected to identify which objects are
 * being published and should match the value returned by the subscription
 * methods.
 */
static VALUE
graphql_publish(VALUE self, VALUE subject, VALUE event) {
    const char		*subj;
    struct _agooErr	err = AGOO_ERR_INIT;

    rb_check_type(subject, T_STRING);
    subj = StringValuePtr(subject);

    if (AGOO_ERR_OK != agoo_server_gpublish(&err, subj, (gqlRef)event)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    return Qnil;
}

static VALUE
rescue_build_header(VALUE x, VALUE ignore) {
    bhArgs		args = (bhArgs)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);

    agoo_err_set(args->err, AGOO_ERR_EVAL, "%s", rb_string_value_ptr(&msg));

    return Qnil;
}

static int
build_headers_cb(VALUE key, VALUE value, VALUE x) {
    bhArgs		args = (bhArgs)x;
    const char		*ks = rb_string_value_ptr((VALUE*)&key);
    volatile VALUE	vs = rb_obj_as_string(value);
    const char		*s = rb_string_value_ptr((VALUE*)&vs);

    if (NULL == (args->headers = gql_add_header(args->err, args->headers, ks, s))) {
	rb_raise(rb_eStandardError, "%s", args->err->msg);
    }
    return ST_CONTINUE;
}

static VALUE
inner_build_headers(VALUE x) {
    volatile VALUE	hh = rb_funcall(build_headers_func, call_id, 1, request_wrap(((bhArgs)x)->req));

    rb_hash_foreach(hh, build_headers_cb, x);

    return Qnil;
}

static void*
protected_build_headers(void *x) {
    return (void*)rb_rescue2(inner_build_headers, (VALUE)x, rescue_build_header, (VALUE)x, rb_eException, (VALUE)0);
}

static agooText
build_headers(agooErr err, agooReq req, agooText headers) {
    struct _bhArgs	args = {
	.err = err,
	.req = req,
	.headers = headers,
    };
    rb_thread_call_with_gvl(protected_build_headers, &args);

    return args.headers;
}

/* Document-method: build_headers=
 *
 * call-seq: build_headers=(func)
 *
 * Provide a function to call that builds headers for GraphQL responses. The
 * function should expect a single request and should return a Hash of the
 * headers to add. Content-Type and Content-Length should not be set.
 */
static VALUE
graphql_build_headers(VALUE self, VALUE func) {
    gql_build_headers = build_headers;
    build_headers_func = func;
    rb_gc_register_address(&build_headers_func);

    return Qnil;
}

static int
headers_cb(VALUE key, VALUE value, VALUE x) {
    agooText		*tp = (agooText*)x;
    const char		*ks = rb_string_value_ptr((VALUE*)&key);
    volatile VALUE	vs = rb_obj_as_string(value);
    const char		*s = rb_string_value_ptr((VALUE*)&vs);
    struct _agooErr	err = AGOO_ERR_INIT;

    if (NULL == (*tp = gql_add_header(&err, *tp, ks, s))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    return ST_CONTINUE;
}


/* Document-method: headers
 *
 * call-seq: headers(header_hash)
 *
 * Provide a Hash to be used as the headers for GraphQL responses.
 * Content-Type and Content-Length should not be set.
 */
static VALUE
graphql_headers(VALUE self, VALUE map) {
    agooText	t = agoo_text_allocate(1024);

    rb_hash_foreach(map, headers_cb, (VALUE)&t);
    gql_headers = t;

    return Qnil;
}

/* Document-class: Agoo::Graphql
 *
 * The Agoo::GraphQL class provides support for the GraphQL API as defined in
 * https://facebook.github.io/graphql/June2018. The approach taken supporting
 * GraphQL with Ruby is to keep the integration as simple as possible. With
 * that in mind there are not new languages or syntax to learn. GraphQL types
 * are defined with SDL which is the language used in the specification. Ruby,
 * is well, just Ruby. A GraphQL type is assigned a Ruby class that implements
 * that type. Thats it. A GraphQL directive or a Ruby method is used to create
 * this association. After that the Agoo server does the work and calls the
 * Ruby object methods as needed to satisfy the GraphQL queries.
 */
void
graphql_init(VALUE mod) {
    graphql_class = rb_define_class_under(mod, "GraphQL", rb_cObject);

    rb_define_module_function(graphql_class, "schema", graphql_schema, 1);

    rb_define_module_function(graphql_class, "load", graphql_load, 1);
    rb_define_module_function(graphql_class, "load_file", graphql_load_file, 1);

    rb_define_module_function(graphql_class, "sdl_dump", graphql_sdl_dump, 1);

    rb_define_module_function(graphql_class, "sdl_types", graphql_sdl_types, 0);

    rb_define_module_function(graphql_class, "publish", graphql_publish, 2);

    rb_define_module_function(graphql_class, "build_headers=", graphql_build_headers, 1);
    rb_define_module_function(graphql_class, "headers", graphql_headers, 1);

    arg_clas = rb_const_get_at(graphql_class, rb_intern("Arg"));
    field_clas = rb_const_get_at(graphql_class, rb_intern("Field"));
    type_clas = rb_const_get_at(graphql_class, rb_intern("Type"));

    call_id = rb_intern("call");
}
