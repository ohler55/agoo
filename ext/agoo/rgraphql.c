// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include <ruby.h>

#include "err.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

static VALUE	graphql_class = Qundef;
static VALUE	graphql_root = Qundef;

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

/* Document-method: schema
 *
 * call-seq: schema(root) { }
 *
 * Start the GraphQL/Ruby integration but assigning a root Ruby object to the
 * GraphQL environment. Withing the block passed to the function SDL strings
 * or files can be loaded. On exiting the block validation of the loaded
 * schema is performed.
 *
 * Note that the _@ruby_ directive is added to the _schema_ type as well as
 * the _Query_, _Mutation_, and _Subscriptio_ type based on the _root_
 * class. Any _@ruby_ directives on the object types in the SDL will also
 * associate a GraphQL and Ruby class.
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
    graphql_root = root;
    rb_gc_register_address(&graphql_root);

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
    
    // TBD set somewhere in graphql side as void* root/schema object

    if (rb_block_given_p()) {
	rb_yield_values2(0, NULL);
    }
    if (AGOO_ERR_OK != gql_validate(&err)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
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

    if (Qundef == graphql_root) {
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
    
    if (Qundef == graphql_root) {
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
