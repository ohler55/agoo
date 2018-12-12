// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>

#include <ruby.h>

#include "err.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

static VALUE	graphql_class = Qundef;
static VALUE	graphql_root = Qundef;

/* Document-method: schema
 *
 * call-seq: schema(root) { }
 *
 * TBD
 */
static VALUE
graphql_schema(VALUE self, VALUE root) {
    struct _agooErr	err = AGOO_ERR_INIT;
    gqlDir		dir;
    
    if (!rb_block_given_p()) {
	rb_raise(rb_eStandardError, "A block is required.");
    }
    if (AGOO_ERR_OK != gql_init(&err)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    
    if (NULL == (dir = gql_directive_create(&err, "ruby", "Associates a Ruby class with a GraphQL type.", 0))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (NULL == gql_dir_arg(&err, dir, "class", &gql_string_type, "The Ruby class to link to the GraphQL type.", 0, NULL, true)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (AGOO_ERR_OK != gql_directive_on(&err, dir, "schema", 6)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    graphql_root = root;
    rb_gc_register_address(&graphql_root);
    
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
 * TBD
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
 * TBD
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
 * TBD
 */
static VALUE
graphql_sdl_dump(VALUE self, VALUE options) {
    agooText		t = agoo_text_allocate(4096);
    volatile VALUE	dump;
    VALUE		v;
    bool		with_desc = true;
    bool		all = false;
    
    Check_Type(options, T_HASH);

    v = rb_hash_aref(options, rb_intern("with_descriptions"));
    if (Qnil != v) {
	with_desc = (Qtrue == v);
    }
    v = rb_hash_aref(options, rb_intern("all"));
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
 * 
 * TBD
 */
void
graphql_init(VALUE mod) {
    graphql_class = rb_define_class_under(mod, "GraphQL", rb_cObject);

    rb_define_module_function(graphql_class, "schema", graphql_schema, 1);

    rb_define_module_function(graphql_class, "load", graphql_load, 1);
    rb_define_module_function(graphql_class, "load_file", graphql_load_file, 1);

    rb_define_module_function(graphql_class, "sdl_dump", graphql_sdl_dump, 1);
}
