// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gqlintro.h"
#include "gqlvalue.h"
#include "graphql.h"

/*
static gqlType	schema_type;
static gqlType	type_type;
static gqlType	type_kind_type;
static gqlType	field_type;
static gqlType	input_value_type;
static gqlType	enum_value_type;
static gqlType	directive_type;
static gqlType	directive_location_type;

*/

// type __Schema {
//   types: [__Type!]!
//   queryType: __Type!
//   mutationType: __Type
//   subscriptionType: __Type
//   directives: [__Directive!]!
// }
static gqlRef
schema_types_resolve(gqlRef target, const char *fieldName, gqlKeyVal *args) {
    // TBD
    return NULL;
}

static gqlRef
schema_query_type_resolve(gqlRef target, const char *fieldName, gqlKeyVal *args) {
    // TBD return schema_type.query.type
    //  lookup "schema" type
    //  get query field
    //  get return type of field
    return NULL;
}

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
	
	NULL == gql_type_field(err, type, "types", type_list, NULL, NULL, 0, true, schema_types_resolve) ||
	NULL == gql_type_field(err, type, "queryType", type_type, NULL, NULL, 0, true, schema_query_type_resolve) ||
	NULL == gql_type_field(err, type, "mutationType", type_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "subscriptionType", type_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "directives", dir_list, NULL, NULL, 0, true, NULL)) {

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

	NULL == gql_type_field(err, type, "kind", kind_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == (fields = gql_type_field(err, type, "fields", field_list, NULL, NULL, 0, false, NULL)) ||
	NULL == gql_type_field(err, type, "interfaces", type_list, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "possibleTypes", type_list, NULL, NULL, 0, false, NULL) ||
	NULL == (enum_values = gql_type_field(err, type, "enumValues", enum_list, NULL, NULL, 0, false, NULL)) ||
	NULL == gql_type_field(err, type, "inputFields", input_list, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "ofType", type, NULL, NULL, 0, false, NULL)) {

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

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "args", input_list, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "type", type_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "isDeprecated", &gql_bool_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "reason", &gql_string_type, NULL, NULL, 0, false, NULL)) {

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

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "type", type_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "defaultValue", &gql_string_type, NULL, NULL, 0, false, NULL)) {

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
	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "isDeprecated", &gql_bool_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "deprecationReason", &gql_string_type, NULL, NULL, 0, false, NULL)) {

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

	NULL == gql_type_field(err, type, "name", &gql_string_type, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "description", &gql_string_type, NULL, NULL, 0, false, NULL) ||
	NULL == gql_type_field(err, type, "location", loc_list, NULL, NULL, 0, true, NULL) ||
	NULL == gql_type_field(err, type, "args", input_list, NULL, NULL, 0, true, NULL)) {

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
