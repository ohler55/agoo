// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "gqlintro.h"
#include "gqlvalue.h"
#include "graphql.h"

static gqlType	schema_type;
static gqlType	type_type;
static gqlType	type_kind_type;
static gqlType	field_type;
static gqlType	input_object_type;
static gqlType	enum_value_type;
static gqlType	directive_type;
static gqlType	directive_location_type;

// type __Schema {
//   types: [__Type!]!
//   queryType: __Type!
//   mutationType: __Type
//   subscriptionType: __Type
//   directives: [__Directive!]!
// }
static void
schema_types_op(gqlValue target, gqlField field, gqlLink args, gqlBuildFunc builder, void *ctx) {
    // TBD iterate over types cache
    //  this doesn't work well for getting just the desired fields
}

static int
create_schema_type(Err err) {
    if (NULL == (schema_type = gql_type_create(err, "__Schema", NULL, true, NULL)) ||
	NULL == gql_type_field(err, schema_type, "types", type_type, NULL, true, true, schema_types_op, NULL) ||
	NULL == gql_type_field(err, schema_type, "queryType", type_type, NULL, true, false, NULL, NULL) ||
	NULL == gql_type_field(err, schema_type, "mutationType", type_type, NULL, false, false, NULL, NULL) ||
	NULL == gql_type_field(err, schema_type, "subscriptionType", type_type, NULL, false, false, NULL, NULL) ||
	NULL == gql_type_field(err, schema_type, "directives", directive_type, NULL, true, true, NULL, NULL)) {

	return err->code;
    }
    return ERR_OK;
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
create_type_type(Err err) {
    type_type = gql_type_create(err, "__Type", NULL, true, NULL);

    if (NULL == type_type) {
	return err->code;
    }
    // TBD add fields
    
    return ERR_OK;
}

static int
create_type_kind_type(Err err) {
    const char	*choices[] = { "SCALAR", "OBJECT", "INTERFACE", "UNION", "ENUM", "INPUT_OBJECT", "LIST", "NON_NULL", NULL };

    type_kind_type = gql_enum_create(err, "__TypeKind", NULL, true, choices);
    
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
create_field_type(Err err) {
    field_type = gql_type_create(err, "__Field", NULL, true, NULL);

    if (NULL == field_type) {
	return err->code;
    }
    // TBD add fields
    
    return ERR_OK;
}

// type __InputValue {
//   name: String!
//   description: String
//   type: __Type!
//   defaultValue: String
// }
static int
create_input_type(Err err) {
    input_object_type = gql_type_create(err, "__InputValue", NULL, true, NULL);

    if (NULL == input_object_type) {
	return err->code;
    }
    // TBD add fields
    
    return ERR_OK;
}

// type __EnumValue {
//   name: String!
//   description: String
//   isDeprecated: Boolean!
//   deprecationReason: String
// }
static int
create_enum_type(Err err) {
    enum_value_type = gql_type_create(err, "__EnumValue", NULL, true, NULL);

    if (NULL == enum_value_type) {
	return err->code;
    }
    // TBD add fields
    
    return ERR_OK;
}

// type __Directive {
//   name: String!
//   description: String
//   locations: [__DirectiveLocation!]!
//   args: [__InputValue!]!
// }
static int
create_directive_type(Err err) {
    directive_type = gql_type_create(err, "__Directive", NULL, true, NULL);

    if (NULL == directive_type) {
	return err->code;
    }
    // TBD add fields
    
    return ERR_OK;
}

static int
create_directive_location_type(Err err) {
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

    directive_location_type = gql_enum_create(err, "__DirectiveLocation", NULL, true, choices);
    
    return err->code;
}
    
int
gql_intro_init(Err err) {
    if (ERR_OK != create_type_kind_type(err) ||
	ERR_OK != create_input_type(err) ||
	ERR_OK != create_type_type(err) ||
	ERR_OK != create_enum_type(err) ||
	ERR_OK != create_field_type(err) ||
	ERR_OK != create_directive_location_type(err) ||
	ERR_OK != create_directive_type(err) ||
	ERR_OK != create_schema_type(err)) {
	return err->code;
    }
    return ERR_OK;
}
