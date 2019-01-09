// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdint.h>
#include <stdlib.h>

#include <ruby.h>
#include <ruby/thread.h>

#include "err.h"
#include "gqleval.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

static VALUE	graphql_class = Qundef;

typedef struct _eval {
    gqlDoc	doc;
    agooErr	err;
    gqlValue	value;
} *Eval;

typedef struct _typeClass {
    gqlType	type;
    const char	*classname;
} *TypeClass;

static TypeClass	type_class_map = NULL;

static void
make_ruby_use(VALUE root, const char *method, const char *type_name) {
    struct _agooErr	err = AGOO_ERR_INIT;
    gqlType		type;
    gqlDirUse		use;
    volatile VALUE	v = rb_funcall(root, rb_intern(method), 0);

    if (Qnil == v) {
	return;
    }
    if (NULL == (type = gql_type_get(type_name))) {
	rb_raise(rb_eStandardError, "Failed to find the '%s' type.", type_name);
    }
    if (NULL == (use = gql_dir_use_create(&err, "ruby"))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (AGOO_ERR_OK != gql_dir_use_arg(&err, use, "class", gql_string_create(&err, rb_obj_classname(v), 0))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    gql_type_directive_use(type, use);
}

static VALUE
rescue_error(VALUE x) {
    Eval		eval = (Eval)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);

    agoo_err_set(eval->err, AGOO_ERR_EVAL, "%s: %s", classname, ms);

    return Qfalse;
}

static VALUE
call_eval(void *x) {
    Eval	eval = (Eval)x;

    eval->value = gql_doc_eval(eval->err, eval->doc);

    return Qnil;
}

static void*
protect_eval(void *x) {
    rb_rescue2(call_eval, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);

    return NULL;
}

static gqlValue
eval_wrap(agooErr err, gqlDoc doc) {
    struct _eval	eval = {
	.doc = doc,
	.err = err,
	.value = NULL,
    };
    
    rb_thread_call_with_gvl(protect_eval, &eval);

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
	    rval = rb_str_new_cstr(gql_string_get(value));
	    break;
	case GQL_SCALAR_TIME: {
	    time_t	secs = (time_t)(value->time / 1000000000LL);
	    
	    rval = rb_time_nano_new(secs, (long)(value->time - secs * 1000000000LL));
	    break;
	}
	case GQL_SCALAR_UUID:
	    // TBD value->uuid.hi or lo
	    break;
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

static gqlRef
resolve(agooErr err, gqlRef target, const char *field_name, gqlKeyVal args) {
    volatile VALUE	result;

    if (NULL != args && NULL != args->key) {
	volatile VALUE	rargs = rb_hash_new();

	for (; NULL != args->key; args++) {
	    rb_hash_aset(rargs, rb_str_new_cstr(args->key), gval_to_ruby(args->value));
	}
	result = rb_funcall((VALUE)target, rb_intern(field_name), 1, rargs);
    } else {
	result = rb_funcall((VALUE)target, rb_intern(field_name), 0);
    }
    return (gqlRef)result;
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

    if (NULL == type) {
	// This is really an error but make a best effort anyway.
	switch (rb_type((VALUE)ref)) {
	case RUBY_T_FLOAT:
	    value = gql_float_create(err, rb_num2dbl(rb_to_float((VALUE)ref)));
	    break;
	case RUBY_T_STRING:
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), RSTRING_LEN(v));
	    break;
	case RUBY_T_ARRAY: {
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
		value = gql_int_create(err, i);
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
	    // TBD UUID
	    break;
	}
	default:
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), RSTRING_LEN(v));
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
	    v = ref_to_string(ref);
	    value = gql_string_create(err, rb_string_value_ptr(&v), RSTRING_LEN(v));
	    break;
	case GQL_SCALAR_TIME: {
	    VALUE	clas = rb_obj_class((VALUE)ref);

	    if (rb_cTime == clas) {
		value = time_to_time(err, (VALUE)ref);
	    }
	    break;
	}
	case GQL_SCALAR_UUID:
	    // TBD include a uuid or use ruby
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
	value = gql_string_create(err, rb_string_value_ptr(&v), RSTRING_LEN(v));
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

static void
build_type_class_map() {
    int		cnt = 0;
    
    free(type_class_map);
    type_class_map = NULL;

    gql_type_iterate(ruby_types_cb, &cnt);

    if (NULL == (type_class_map = (TypeClass)malloc(sizeof(struct _typeClass) * (cnt + 1)))) {
	rb_raise(rb_eNoMemError, "out of memory");
    }
    memset(type_class_map, 0, sizeof(struct _typeClass) * (cnt + 1));
    cnt = 0;
    gql_type_iterate(ruby_types_cb, &cnt);
}

static int
iterate(agooErr err, gqlRef ref, int (*cb)(agooErr err, gqlRef ref, void *ctx), void *ctx) {
    VALUE	a = (VALUE)ref;
    int		cnt = (int)RARRAY_LEN(a);
    int		i;
    
    for (i = 0; i < cnt; i++) {
	if (AGOO_ERR_OK != cb(err, (gqlRef)rb_ary_entry(a, i), ctx)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
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
 * the _Query_, _Mutation_, and _Subscriptio_ type based on the _root_
 * class. Any _@ruby_ directives on the object types in the SDL will also
 * associate a GraphQL and Ruby class. The association will be used to
 * validate the Ruby class to verify it implements yhe methods described by
 * the GraphQL type.
 */
static VALUE
graphql_schema(VALUE self, VALUE root) {
    struct _agooErr	err = AGOO_ERR_INIT;
    gqlType		type;
    gqlDir		dir;
    gqlDirUse		use;
    
    if (!rb_block_given_p()) {
	rb_raise(rb_eStandardError, "A block is required.");
    }
    if (AGOO_ERR_OK != gql_init(&err)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (NULL == (dir = gql_directive_create(&err, "ruby", "Associates a Ruby class with a GraphQL type.", 0))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (NULL == gql_dir_arg(&err, dir, "class", &gql_string_type, NULL, 0, NULL, true)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (AGOO_ERR_OK != gql_directive_on(&err, dir, "SCHEMA", 6) ||
	AGOO_ERR_OK != gql_directive_on(&err, dir, "OBJECT", 6)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    gql_root = (gqlRef)root;
    rb_gc_register_address(&root);

    gql_doc_eval_func = eval_wrap;
    gql_resolve_func = resolve;
    gql_coerce_func = coerce;
    gql_type_func = ref_type;
    gql_iterate_func = iterate;

    if (NULL == (use = gql_dir_use_create(&err, "ruby"))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (AGOO_ERR_OK != gql_dir_use_arg(&err, use, "class", gql_string_create(&err, rb_obj_classname(root), 0))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (NULL == (type = gql_type_get("schema"))) {
	rb_raise(rb_eStandardError, "Failed to find the 'schema' type.");
    }
    gql_type_directive_use(type, use);

    make_ruby_use(root, "query", "Query");
    make_ruby_use(root, "mutation", "Mutation");
    make_ruby_use(root, "subscription", "Subscription");

    if (rb_block_given_p()) {
	rb_yield_values2(0, NULL);
    }
    if (AGOO_ERR_OK != gql_validate(&err)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    build_type_class_map();

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
    if (AGOO_ERR_OK != sdl_parse(&err, StringValuePtr(sdl), RSTRING_LEN(sdl))) {
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
    size_t		len;
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
    len = ftell(f);
    sdl = ALLOC_N(char, len + 1);
    fseek(f, 0, SEEK_SET);
    if (len != fread(sdl, 1, len, f)) {
	rb_raise(rb_eIOError, "%s", strerror(errno));
    } else {
	sdl[len] = '\0';
    }
    fclose(f);
    if (AGOO_ERR_OK != sdl_parse(&err, sdl, len)) {
	xfree(sdl);
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    xfree(sdl);

    return Qnil;
}

/* Document-method: dump_sdl
 *
 * call-seq: dump_sdl()
 *
 * The preferred method of inspecting a GraphQL schema is to use introspection
 * queries but for debugging and for reviewing the schema a dump of the schema
 * as SDL can be helpful. The _#dump_sdl_ method returns the schema as an SDL
 * string.
 *
 * - *options* [_Hash_] server options
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
    t = gql_schema_sdl(t, with_desc, all);

    dump = rb_str_new(t->text, t->len);
    agoo_text_release(t);
    
    return dump;
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
}
