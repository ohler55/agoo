// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gqlcobj.h"
#include "gqlintro.h"
#include "gqlvalue.h"
#include "graphql.h"

// type __Schema {
//   types: [__Type!]!
//   queryType: __Type!
//   mutationType: __Type
//   subscriptionType: __Type
//   directives: [__Directive!]!
// }
static int
create_schema_type(agooErr err) {
    gqlType	type;
    gqlType	type_type;
    gqlType	type_list;
    gqlType	dir_type;
    gqlType	dir_list;
    
    if (NULL == (type = gql_type_create(err, "__Schema", NULL, 0, NULL)) ||

	NULL == (type_type = gql_assure_type(err, "__Type")) ||
	NULL == (type_list = gql_assure_list(err, type_type, true)) ||
	NULL == (dir_type = gql_assure_type(err, "__Directive")) ||
	NULL == (dir_list = gql_assure_list(err, dir_type, true)) ||
	
	NULL == gql_type_field(err, type, "types", type_list, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "queryType", type_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "mutationType", type_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "subscriptionType", type_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "directives", dir_list, NULL, NULL, 0, true)) {

	return err->code;
    }
    type->core = true;
    
    return AGOO_ERR_OK;
}

// type __Type {
//   kind: __TypeKind!
//   name: String
//   description: String
//   fields(includeDeprecated: Boolean = false): [__Field!]
//   interfaces: [__Type!]
//   possibleTypes: [__Type!]
//   enumValues(includeDeprecated: Boolean = false): [__EnumValue!]
//   inputFields: [__InputValue!]
//   ofType: __Type
// }
static int
create_type_type(agooErr err) {
    gqlType	type;
    gqlField	fields = NULL;
    gqlField	enum_values = NULL;
    gqlValue	dv;
    gqlType	kind_type;
    gqlType	type_list;
    gqlType	field_type;
    gqlType	field_list;
    gqlType	enum_type;
    gqlType	enum_list;
    gqlType	input_type;
    gqlType	input_list;
    
    if (NULL == (type = gql_type_create(err, "__Type", NULL, 0, NULL)) ||

	NULL == (type_list = gql_assure_list(err, type, true)) ||
	NULL == (field_type = gql_assure_type(err, "__Field")) ||
	NULL == (field_list = gql_assure_list(err, field_type, true)) ||
	NULL == (kind_type = gql_assure_type(err, "__TypeKind")) ||
	NULL == (enum_type = gql_assure_type(err, "__EnumValue")) ||
	NULL == (enum_list = gql_assure_list(err, enum_type, true)) ||
	NULL == (input_type = gql_assure_type(err, "__InputValue")) ||
	NULL == (input_list = gql_assure_list(err, input_type, true)) ||

	NULL == gql_type_field(err, type, "kind", kind_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == (fields = gql_type_field(err, type, "fields", field_list, NULL, NULL, 0, false)) ||
	NULL == gql_type_field(err, type, "interfaces", type_list, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "possibleTypes", type_list, NULL, NULL, 0, false) ||
	NULL == (enum_values = gql_type_field(err, type, "enumValues", enum_list, NULL, NULL, 0, false)) ||
	NULL == gql_type_field(err, type, "inputFields", input_list, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "ofType", type, NULL, NULL, 0, false)) {

	return err->code;
    }
    type->core = true;

    if (NULL == (dv = gql_bool_create(err, false)) ||
	
	NULL == gql_field_arg(err, fields, "includeDeprecated", &gql_bool_type, NULL, 0, dv, false) ||
	NULL == (dv = gql_bool_create(err, false)) ||
	NULL == gql_field_arg(err, enum_values, "includeDeprecated", &gql_bool_type, NULL, 0, dv, false)) {

	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
create_type_kind_type(agooErr err) {
    const char	*choices[] = {
	"SCALAR",
	"OBJECT",
	"INTERFACE",
	"UNION",
	"ENUM",
	"INPUT_OBJECT",
	"LIST",
	"NON_NULL",
	NULL
    };
    const char	**cp;
    gqlType	type;

    if (NULL == (type = gql_enum_create(err, "__TypeKind", NULL, 0))) {
	return err->code;
    }
    type->core = true;

    for (cp = choices; NULL != *cp; cp++) {
	if (NULL == gql_enum_append(err, type, *cp, 0, NULL, 0)) {
	    return err->code;
	}
    }
    return err->code;
}

// type __Field {
//   name: String!
//   description: String
//   args: [__InputValue!]!
//   type: __Type!
//   isDeprecated: Boolean!
//   deprecationReason: String
// }
static int
create_field_type(agooErr err) {
    gqlType	type;
    gqlType	type_type;
    gqlType	input_type;
    gqlType	input_list;
    
    if (NULL == (type = gql_type_create(err, "__Field", NULL, 0, NULL)) ||

	NULL == (type_type = gql_assure_type(err, "__Type")) ||
	NULL == (input_type = gql_assure_type(err, "__InputValue")) ||
	NULL == (input_list = gql_assure_list(err, input_type, true)) ||

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "args", input_list, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "type", type_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "isDeprecated", &gql_bool_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "reason", &gql_string_type, NULL, NULL, 0, false)) {

	return err->code;
    }
    type->core = true;
    
    return AGOO_ERR_OK;
}

// type __InputValue {
//   name: String!
//   description: String
//   type: __Type!
//   defaultValue: String
// }
static int
create_input_type(agooErr err) {
    gqlType	type;
    gqlType	type_type;

    if (NULL == (type = gql_type_create(err, "__InputValue", NULL, 0, NULL)) ||

	NULL == (type_type = gql_assure_type(err, "__Type")) ||

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "type", type_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "defaultValue", &gql_string_type, NULL, NULL, 0, false)) {

	return err->code;
    }
    type->core = true;
    
    return AGOO_ERR_OK;
}

// type __EnumValue {
//   name: String!
//   description: String
//   isDeprecated: Boolean!
//   deprecationReason: String
// }
static int
create_enum_type(agooErr err) {
    gqlType	type;

    if (NULL == (type = gql_type_create(err, "__EnumValue", NULL, 0, NULL)) ||
	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "isDeprecated", &gql_bool_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "deprecationReason", &gql_string_type, NULL, NULL, 0, false)) {

	return err->code;
    }
    type->core = true;
    
    return AGOO_ERR_OK;
}

// type __Directive {
//   name: String!
//   description: String
//   locations: [__DirectiveLocation!]!
//   args: [__InputValue!]!
// }
static int
create_directive_type(agooErr err) {
    gqlType	type;
    gqlType	input_type;
    gqlType	input_list;
    gqlType	loc_type;
    gqlType	loc_list;
 
    if (NULL == (type = gql_type_create(err, "__Directive", NULL, 0, NULL)) ||

	NULL == (input_type = gql_assure_type(err, "__InputValue")) ||
	NULL == (input_list = gql_assure_list(err, input_type, true)) ||
	NULL == (loc_type = gql_assure_type(err, "__DirectiveLocation")) ||
	NULL == (loc_list = gql_assure_list(err, loc_type, true)) ||

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false) ||
	NULL == gql_type_field(err, type, "location", loc_list, NULL, NULL, 0, true) ||
	NULL == gql_type_field(err, type, "args", input_list, NULL, NULL, 0, true)) {

	return err->code;
    }
    type->core = true;
    
    return AGOO_ERR_OK;
}

static int
create_directive_location_type(agooErr err) {
    const char	*choices[] = {
	"QUERY",
	"MUTATION",
	"SUBSCRIPTION",
	"FIELD",
	"FRAGMENT_DEFINITION",
	"FRAGMENT_SPREAD",
	"INLINE_FRAGMENT",
	"SCHEMA",
	"SCALAR",
	"OBJECT",
	"FIELD_DEFINITION",
	"ARGUMENT_DEFINITION",
	"INTERFACE",
	"UNION",
	"ENUM",
	"ENUM_VALUE",
	"INPUT_OBJECT",
	"INPUT_FIELD_DEFINITION",
	NULL };
    const char	**cp;
    gqlType	type;

    if (NULL == (type = gql_enum_create(err, "__DirectiveLocation", NULL, 0))) {
	return err->code;
    }
    type->core = true;
    for (cp = choices; NULL != *cp; cp++) {
	if (NULL == gql_enum_append(err, type, *cp, 0, NULL, 0)) {
	    return err->code;
	}
    }
    return err->code;
}

static int
create_dir_skip(agooErr err) {
    gqlDir	dir = gql_directive_create(err, "skip", NULL, 0);

    if (NULL == dir) {
	return err->code;
    }
    dir->core = true;
    if (AGOO_ERR_OK != gql_directive_on(err, dir, "FIELD", -1) ||
	AGOO_ERR_OK != gql_directive_on(err, dir, "FRAGMENT_SPREAD", -1) ||
	AGOO_ERR_OK != gql_directive_on(err, dir, "INLINE_FRAGMENT", -1) ||
	NULL == gql_dir_arg(err, dir, "if", &gql_bool_type, NULL, -1, NULL, true)) {

	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
create_dir_include(agooErr err) {
    gqlDir	dir = gql_directive_create(err, "include", NULL, 0);

    if (NULL == dir) {
	return err->code;
    }
    dir->core = true;
    if (AGOO_ERR_OK != gql_directive_on(err, dir, "FIELD", -1) ||
	AGOO_ERR_OK != gql_directive_on(err, dir, "FRAGMENT_SPREAD", -1) ||
	AGOO_ERR_OK != gql_directive_on(err, dir, "INLINE_FRAGMENT", -1) ||
	NULL == gql_dir_arg(err, dir, "if", &gql_bool_type, NULL, 0, NULL, true)) {

	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
create_dir_deprecated(agooErr err) {
    gqlDir	dir = gql_directive_create(err, "deprecated", NULL, 0);
    gqlValue	dv;

    if (NULL == dir) {
	return err->code;
    }
    dir->core = true;
    if (AGOO_ERR_OK != gql_directive_on(err, dir, "FIELD_DEFINITION", -1) ||
	AGOO_ERR_OK != gql_directive_on(err, dir, "ENUM_VALUE", -1) ||
	NULL == (dv = gql_string_create(err, "No longer supported", -1)) ||
	NULL == gql_dir_arg(err, dir, "reason", &gql_string_type, NULL, -1, dv, false)) {

	return err->code;
    }
    return AGOO_ERR_OK;
}

int
gql_intro_init(agooErr err) {
    if (AGOO_ERR_OK != create_type_kind_type(err) ||
	AGOO_ERR_OK != create_input_type(err) ||
	AGOO_ERR_OK != create_type_type(err) ||
	AGOO_ERR_OK != create_enum_type(err) ||
	AGOO_ERR_OK != create_field_type(err) ||
	AGOO_ERR_OK != create_directive_location_type(err) ||
	AGOO_ERR_OK != create_directive_type(err) ||
	AGOO_ERR_OK != create_schema_type(err) ||
	AGOO_ERR_OK != create_dir_deprecated(err) ||
	AGOO_ERR_OK != create_dir_include(err) ||
	AGOO_ERR_OK != create_dir_skip(err)) {

	return err->code;
    }
    return AGOO_ERR_OK;
}

// introspection handlers /////////////////////////////////////////////////////////////////////

// __Field
// name: String
static gqlRef
intro_field_name(agooErr err, gqlCobj obj, gqlKeyVal args) {
    return (gqlRef)gql_string_create(err, ((gqlField)obj->ref)->name, -1);
}

static struct _gqlCmethod	field_methods[] = {
    { .key = "name",          .func = intro_field_name },
    { .key = NULL,            .func = NULL },
};

static struct _gqlCclass	field_class = {
    .name = "__Field",
    .methods = field_methods,
};

gqlCobj
gql_field_intro_create(agooErr err, gqlField field) {
    return gql_c_obj_create(err, (gqlRef)field, &field_class);
}

// __InputValue

// EnumValue

// __Directive



// kind: __TypeKind!
static gqlRef
intro_type_kind(agooErr err, gqlCobj obj, gqlKeyVal args) {
    gqlType	type = (gqlType)obj->ref;
    const char	*kind = NULL;
    
    switch (type->kind) {
    case GQL_OBJECT:	kind = "OBJECT";	break;
    case GQL_INPUT:	kind = "INPUT_OBJECT";	break;
    case GQL_UNION:	kind = "UNION";		break;
    case GQL_INTERFACE:	kind = "INTERFACE";	break;
    case GQL_ENUM:	kind = "ENUM";		break;
    case GQL_SCALAR:	kind = "SCALAR";	break;
    case GQL_LIST:	kind = "LIST";		break;
    default:
	agoo_err_set(err, AGOO_ERR_ARG, "__Type kind field not valid. %s:%d", __FILE__, __LINE__);
	return NULL;
    }
    return (gqlRef)gql_token_create(err, kind, -1, gql_type_get("__TypeKind"));
}

// name: String
static gqlRef
intro_type_name(agooErr err, gqlCobj obj, gqlKeyVal args) {
    return (gqlRef)gql_string_create(err, ((gqlType)obj->ref)->name, -1);
}

// description: String
static gqlRef
intro_type_description(agooErr err, gqlCobj obj, gqlKeyVal args) {
    gqlType	type = (gqlType)obj->ref;
    
    if (NULL == type->desc) {
	return (gqlRef)gql_null_create(err);
    }
    return (gqlRef)gql_string_create(err, ((gqlType)obj->ref)->desc, -1);
}

// OBJECT and INTERFACE only
// fields(includeDeprecated: Boolean = false): [__Field!]
static gqlRef
intro_type_fields(agooErr err, gqlCobj obj, gqlKeyVal args) {
    gqlType	type = (gqlType)obj->ref;
    
    // TBD get deprecated flag

    
    
    // TBD

    return NULL;
}

// OBJECT only
// interfaces: [__Type!]
static gqlRef
intro_type_interfaces(agooErr err, gqlCobj obj, gqlKeyVal args) {

    // TBD if an object then form a list and and add each interface type
    //  type->iterfaces
    //  need a c-obj for that, how to destroy those when no longer needed?
    // maybe add object to list type type

    return NULL;
}

// INTERFACE and UNION only
// possibleTypes: [__Type!]
static gqlRef
intro_type_possible_types(agooErr err, gqlCobj obj, gqlKeyVal args) {

    // TBD

    return NULL;
}

// ENUM only
// enumValues(includeDeprecated: Boolean = false): [__EnumValue!]
static gqlRef
intro_type_enum_values(agooErr err, gqlCobj obj, gqlKeyVal args) {

    // TBD

    return NULL;
}

// INPUT_OBJECT only
// inputFields: [__InputValue!]
static gqlRef
intro_type_input_fields(agooErr err, gqlCobj obj, gqlKeyVal args) {

    // TBD

    return NULL;
}

// NON_NULL and LIST only
// ofType: __Type
static gqlRef
intro_type_of_type(agooErr err, gqlCobj obj, gqlKeyVal args) {
    gqlType	type = (gqlType)obj->ref;

    if (NULL == type || NULL == type->base || (GQL_LIST != type->kind && GQL_NON_NULL != type->kind)) {
	return NULL;
    }
    return (gqlRef)type->base->intro;
}

static struct _gqlCmethod	type_methods[] = {
    { .key = "kind",          .func = intro_type_kind },
    { .key = "name",          .func = intro_type_name },
    { .key = "description",   .func = intro_type_description },
    { .key = "fields",        .func = intro_type_fields },
    { .key = "interfaces",    .func = intro_type_interfaces },
    { .key = "possibleTypes", .func = intro_type_possible_types },
    { .key = "enumValues",    .func = intro_type_enum_values },
    { .key = "inputFields",   .func = intro_type_input_fields },
    { .key = "ofType",        .func = intro_type_of_type },
    { .key = NULL,            .func = NULL },
};

// __Type
static struct _gqlCclass	type_class = {
    .name = "__Type",
    .methods = type_methods,
};

gqlCobj
gql_type_intro_create(agooErr err, gqlType type) {
    return gql_c_obj_create(err, (gqlRef)type, &type_class);
}

// Introspection Query Root

static gqlRef
intro_root_type(agooErr err, gqlCobj obj, gqlKeyVal args) {
    const char	*name = gql_string_get(gql_get_arg_value(args, "name"));
    gqlType	type;
    
    if (NULL == name) {
	agoo_err_set(err, AGOO_ERR_ARG, "__type field requires a name argument. %s:%d", __FILE__, __LINE__);
	return NULL;
    }
    if (NULL == (type = gql_type_get(name))) {
	agoo_err_set(err, AGOO_ERR_ARG, "%s is not a defined type. %s:%d", name, __FILE__, __LINE__);
	return NULL;
    }
    // TBD create cobj instead

    return type->intro;
}

static gqlRef
intro_root_schema(agooErr err, gqlCobj obj, gqlKeyVal args) {
    // TBD return intro_schema_obj
    return NULL;
}

static struct _gqlCmethod	root_methods[] = {
    { .key = "__type",   .func = intro_root_type },
    { .key = "__schema", .func = intro_root_schema },
    { .key = NULL,       .func = NULL },
};

static struct _gqlCclass	root_class = {
    .name = "__Query",
    .methods = root_methods,
};

struct _gqlCobj		gql_intro_query_root = {
    .clas = &root_class,
    .ref = NULL,
};
