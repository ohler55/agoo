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
	NULL == gql_type_field(err, type, "locations", loc_list, NULL, NULL, 0, true) ||
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

static struct _gqlCclass	type_class;

gqlValue
gql_extract_arg(agooErr err, gqlField field, gqlSel sel, const char *key) {
    if (NULL != sel->args) {
	gqlSelArg	sa;
	gqlValue	v = NULL;

	for (sa = sel->args; NULL != sa; sa = sa->next) {
	    if (0 != strcmp(sa->name, key)) {
		continue;
	    }
	    if (NULL != sa->var) {
		v = sa->var->value;
	    } else {
		v = sa->value;
	    }
	    if (NULL != field) {
		gqlArg	fa;

		for (fa = field->args; NULL != fa; fa = fa->next) {
		    if (0 == strcmp(sa->name, fa->name)) {
			if (v->type != fa->type && GQL_SCALAR_VAR != v->type->scalar_kind) {
			    if (AGOO_ERR_OK != gql_value_convert(err, v, fa->type)) {
				return NULL;
			    }
			}
			break;
		    }
		}
	    }
	}
	return v;
    }
    return NULL;
}

static bool
is_deprecated(gqlDirUse use) {
    for (; NULL != use; use = use->next) {
	if (0 == strcmp("deprecated", use->dir->name)) {
	    return true;
	}
    }
    return false;
}

static const char*
deprecation_reason(gqlDirUse use) {
    const char	*reason = NULL;

    for (; NULL != use; use = use->next) {
	if (0 == strcmp("deprecated", use->dir->name)) {
	    gqlLink	a;

	    for (a = use->args; NULL != a; a = a->next) {
		if (0 == strcmp("reason", a->key)) {
		    reason = gql_string_get(a->value);
		    break;
		}
	    }
	}
    }
    return reason;
}

// InputValue
// name: String!
static int
input_value_name(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_string_create(err, ((gqlArg)obj->ptr)->name, -1));
}

// description: String
static int
input_value_description(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;
    const char	*s = ((gqlArg)obj->ptr)->desc;
    gqlValue	desc;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == s) {
	desc = gql_null_create(err);
    } else {
	desc = gql_string_create(err, s, -1);
    }
    return gql_object_set(err, result, key, desc);
}

// type: __Type!
static int
input_value_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlArg		a = (gqlArg)obj->ptr;
    const char		*key = sel->name;
    struct _gqlCobj	child = { .clas = &type_class };
    gqlValue		co;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == a || NULL == a->type) {
	return gql_object_set(err, result, key, gql_null_create(err));
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    child.ptr = (void*)a->type;

    return gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, depth + 1);
}

// defaultValue: String
static int
input_value_default_value(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlArg	a = (gqlArg)obj->ptr;
    const char	*key = sel->name;
    gqlValue	dv = a->default_value;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == dv) {
	return gql_object_set(err, result, key, gql_null_create(err));
    }
    if (NULL == (dv = gql_value_dup(err, dv))) {
	return err->code;
    }
    if (AGOO_ERR_OK != gql_value_convert(err, dv, &gql_string_type)) {
	return err->code;
    }
    return gql_object_set(err, result, key, dv);
}

static struct _gqlCmethod	input_value_methods[] = {
    { .key = "name",              .func = input_value_name },
    { .key = "description",       .func = input_value_description },
    { .key = "type",      .func = input_value_type },
    { .key = "defaultValue", .func = input_value_default_value },
    { .key = NULL,                .func = NULL },
};

static struct _gqlCclass	input_value_class = {
    .name = "__InputValue",
    .methods = input_value_methods,
};

// __Field
// name: String!
static int
field_name(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_string_create(err, ((gqlField)obj->ptr)->name, -1));
}

// description: String
static int
field_description(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;
    const char	*s = ((gqlField)obj->ptr)->desc;
    gqlValue	desc;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == s) {
	desc = gql_null_create(err);
    } else {
	desc = gql_string_create(err, s, -1);
    }
    return gql_object_set(err, result, key, desc);
}

// args: [__InputValue!]!
static int
field_args(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlField		f = (gqlField)obj->ptr;
    const char		*key = sel->name;
    gqlArg		a;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &input_value_class };
    int			d2 = depth + 1;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;

    for (a = f->args; NULL != a; a = a->next) {
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = a;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// type: __Type!
static int
field_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlField		f = (gqlField)obj->ptr;
    const char		*key = sel->name;
    struct _gqlCobj	child = { .clas = &type_class };
    gqlValue		co;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == f || NULL == f->type) {
	co = gql_null_create(err);

	return gql_object_set(err, result, key, co);
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    child.ptr = (void*)f->type;

    return gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, depth + 1);
}

// isDeprecated: Boolean!
static int
field_is_deprecated(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_bool_create(err, is_deprecated(((gqlField)obj->ptr)->dir)));
}

// deprecationReason: String
static int
field_deprecation_reason(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*reason = deprecation_reason(((gqlField)obj->ptr)->dir);
    const char	*key = sel->name;
    gqlValue	rv;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == reason) {
	rv = gql_null_create(err);
    } else {
	rv = gql_string_create(err, reason, -1);
    }
    return gql_object_set(err, result, key, rv);
}

static struct _gqlCmethod	field_methods[] = {
    { .key = "name",              .func = field_name },
    { .key = "description",       .func = field_description },
    { .key = "args",              .func = field_args },
    { .key = "type",              .func = field_type },
    { .key = "isDeprecated",      .func = field_is_deprecated },
    { .key = "deprecationReason", .func = field_deprecation_reason },
    { .key = NULL,                .func = NULL },
};

static struct _gqlCclass	field_class = {
    .name = "__Field",
    .methods = field_methods,
};

// EnumValue
// name: String!
static int
enum_value_name(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_string_create(err, ((gqlEnumVal)obj->ptr)->value, -1));
}

// description: String
static int
enum_value_description(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;
    const char	*s = ((gqlEnumVal)obj->ptr)->desc;
    gqlValue	desc;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == s) {
	desc = gql_null_create(err);
    } else {
	desc = gql_string_create(err, s, -1);
    }
    return gql_object_set(err, result, key, desc);
}

static int
enum_value_is_deprecated(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_bool_create(err, is_deprecated(((gqlEnumVal)obj->ptr)->dir)));
}

static int
enum_value_deprecation_reason(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*reason = deprecation_reason(((gqlEnumVal)obj->ptr)->dir);
    const char	*key = sel->name;
    gqlValue	rv;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == reason) {
	rv = gql_null_create(err);
    } else {
	rv = gql_string_create(err, reason, -1);
    }
    return gql_object_set(err, result, key, rv);
}

static struct _gqlCmethod	enum_value_methods[] = {
    { .key = "name",              .func = enum_value_name },
    { .key = "description",       .func = enum_value_description },
    { .key = "isDeprecated",      .func = enum_value_is_deprecated },
    { .key = "deprecationReason", .func = enum_value_deprecation_reason },
    { .key = NULL,                .func = NULL },
};

static struct _gqlCclass	enum_value_class = {
    .name = "__EnumValue",
    .methods = enum_value_methods,
};

// __Directive
// name: String!
static int
directive_name(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_string_create(err, ((gqlDir)obj->ptr)->name, -1));
}

// description: String
static int
directive_description(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;
    const char	*s = ((gqlDir)obj->ptr)->desc;
    gqlValue	desc;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == s) {
	desc = gql_null_create(err);
    } else {
	desc = gql_string_create(err, s, -1);
    }
    return gql_object_set(err, result, key, desc);
}

// locations: [__DirectiveLocation!]!
static int
directive_locations(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlDir		d = (gqlDir)obj->ptr;
    const char		*key = sel->name;
    gqlStrLink		locs = d->locs;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		c;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    for (; NULL != locs; locs = locs->next) {
	if (NULL == (c = gql_string_create(err, locs->str, -1)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, c)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static int
directive_args(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlDir		d = (gqlDir)obj->ptr;
    const char		*key = sel->name;
    gqlArg		a;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &input_value_class };
    int			d2 = depth + 1;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;

    for (a = d->args; NULL != a; a = a->next) {
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = a;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static struct _gqlCmethod	directive_methods[] = {
    { .key = "name",        .func = directive_name },
    { .key = "description", .func = directive_description },
    { .key = "locations",   .func = directive_locations },
    { .key = "args",        .func = directive_args },
    { .key = NULL,          .func = NULL },
};

static struct _gqlCclass	directive_class = {
    .name = "__Directive",
    .methods = directive_methods,
};

// __Type
// kind: __TypeKind!
static int
type_kind(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*kind = NULL;
    const char	*key = sel->name;

    switch (((gqlType)obj->ptr)->kind) {
    case GQL_OBJECT:	kind = "OBJECT";	break;
    case GQL_INPUT:	kind = "INPUT_OBJECT";	break;
    case GQL_UNION:	kind = "UNION";		break;
    case GQL_INTERFACE:	kind = "INTERFACE";	break;
    case GQL_ENUM:	kind = "ENUM";		break;
    case GQL_SCALAR:	kind = "SCALAR";	break;
    case GQL_LIST:	kind = "LIST";		break;
    default:
	return agoo_err_set(err, AGOO_ERR_ARG, "__Type kind field not valid. %s:%d", __FILE__, __LINE__);
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_token_create(err, kind, -1, gql_type_get("__TypeKind")));
}

// name: String
static int
type_name(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char	*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    return gql_object_set(err, result, key, gql_string_create(err, ((gqlType)obj->ptr)->name, -1));
}

// description: String
static int
type_description(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType	type = (gqlType)obj->ptr;
    const char	*key = sel->name;
    gqlValue	desc;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == type->desc) {
	desc = gql_null_create(err);
    } else {
	desc = gql_string_create(err, type->desc, -1);
    }
    return gql_object_set(err, result, key, desc);
}

// OBJECT and INTERFACE only
// fields(includeDeprecated: Boolean = false): [__Field!]
static int
type_fields(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    gqlField		f;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &field_class };
    int			d2 = depth + 1;
    gqlValue		a = gql_extract_arg(err, field, sel, "includeDeprecated");
    bool		inc_dep = false;

    if (GQL_OBJECT != type->kind && GQL_SCHEMA != type->kind && GQL_INTERFACE != type->kind) {
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL != a && GQL_SCALAR_BOOL == a->type->scalar_kind && a->b) {
	inc_dep = true;
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;
    for (f = type->fields; NULL != f; f = f->next) {
	if (!inc_dep && is_deprecated(f->dir)) {
	    continue;
	}
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = f;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// OBJECT only
// interfaces: [__Type!]
static int
type_interfaces(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    gqlTypeLink    	tl;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &type_class };
    int			d2 = depth + 1;

    if (GQL_OBJECT != type->kind) {
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;
    for (tl = type->interfaces; NULL != tl; tl = tl->next) {
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = tl->type;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// INTERFACE and UNION only
// possibleTypes: [__Type!]
static bool
has_interface(gqlType type, gqlType interface) {
    gqlTypeLink    	tl;

    for (tl = type->interfaces; NULL != tl; tl = tl->next) {
	if (tl->type == type) {
	    return true;
	}
    }
    return false;
}

typedef struct _posCtx {
    agooErr	err;
    gqlDoc	doc;
    gqlSel	sel;
    int		depth;
    gqlValue	list;
    gqlType	interface;
    gqlCobj	child;
    gqlField	cf;
} *PosCtx;

static void
possible_cb(gqlType type, void *ctx) {
    PosCtx	pc = (PosCtx)ctx;

    if (AGOO_ERR_OK != pc->err->code) {
	return;
    }
    if (GQL_LIST != type->kind && has_interface(type, pc->interface)) {
	gqlValue	co;

	if (NULL == (co = gql_object_create(pc->err)) ||
	    AGOO_ERR_OK != gql_list_append(pc->err, pc->list, co)) {
	    return;
	}
	pc->child->ptr = type;
	gql_eval_sels(pc->err, pc->doc, (gqlRef)pc->child, pc->cf, pc->sel->sels, co, pc->depth);
    }
}

static int
type_possible_types(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    gqlTypeLink    	tl;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &type_class };
    int			d2 = depth + 1;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;

    switch (type->kind) {
    case GQL_INTERFACE: {
	struct _posCtx	pc = {
	    .err = err,
	    .doc = doc,
	    .sel = sel,
	    .depth = depth + 1,
	    .list = list,
	    .interface = type,
	    .child = &child,
	    .cf = &cf,
	};

	gql_type_iterate(possible_cb, &pc);
	break;
    }
    case GQL_UNION:
	for (tl = type->types; NULL != tl; tl = tl->next) {
	    if (NULL == (co = gql_object_create(err)) ||
		AGOO_ERR_OK != gql_list_append(err, list, co)) {
		return err->code;
	    }
	    child.ptr = tl->type;
	    if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
		return err->code;
	    }
	}
	break;
    default:
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// ENUM only
// enumValues(includeDeprecated: Boolean = false): [__EnumValue!]
static int
type_enum_values(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    gqlEnumVal		c;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &enum_value_class };
    int			d2 = depth + 1;
    gqlValue		a = gql_extract_arg(err, field, sel, "includeDeprecated");
    bool		inc_dep = false;

    if (GQL_ENUM != type->kind) {
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL != a && GQL_SCALAR_BOOL == a->type->scalar_kind && a->b) {
	inc_dep = true;
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;
    for (c = type->choices; NULL != c; c = c->next) {
	if (!inc_dep && is_deprecated(c->dir)) {
	    continue;
	}
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = c;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// INPUT_OBJECT only
// inputFields: [__InputValue!]
static int
type_input_fields(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    gqlArg		a;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &input_value_class };
    int			d2 = depth + 1;

    if (GQL_INPUT != type->kind) {
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;

    for (a = type->args; NULL != a; a = a->next) {
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = a;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

// NON_NULL and LIST only
// ofType: __Type
static int
type_of_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlType		type = (gqlType)obj->ptr;
    const char		*key = sel->name;
    struct _gqlCobj	child = { .clas = &type_class };
    gqlValue		co;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == type || NULL == type->base || (GQL_LIST != type->kind && GQL_NON_NULL != type->kind)) {
	co = gql_null_create(err);

	return gql_object_set(err, result, key, co);
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    child.ptr = (void*)type->base;

    return gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, depth + 1);
}

static struct _gqlCmethod	type_methods[] = {
    { .key = "kind",          .func = type_kind },
    { .key = "name",          .func = type_name },
    { .key = "description",   .func = type_description },
    { .key = "fields",        .func = type_fields },
    { .key = "interfaces",    .func = type_interfaces },
    { .key = "possibleTypes", .func = type_possible_types },
    { .key = "enumValues",    .func = type_enum_values },
    { .key = "inputFields",   .func = type_input_fields },
    { .key = "ofType",        .func = type_of_type },
    { .key = NULL,            .func = NULL },
};

// __Type
static struct _gqlCclass	type_class = {
    .name = "__Type",
    .methods = type_methods,
};

// __Schema
typedef struct _schemaCbCtx {
    agooErr	err;
    gqlDoc	doc;
    gqlSel	sel;
    int		depth;
    gqlValue	list;
} *SchemaCbCtx;

static void
schema_types_cb(gqlType type, void *ctx) {
    SchemaCbCtx		scc = (SchemaCbCtx)ctx;
    gqlValue		co;
    struct _gqlCobj	child = { .clas = &type_class, .ptr = (void*)type };
    struct _gqlField	cf;

    if (AGOO_ERR_OK != scc->err->code || GQL_LIST == type->kind) {
	return;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = scc->sel->type->base;

    if (NULL == (co = gql_object_create(scc->err)) ||
	AGOO_ERR_OK != gql_list_append(scc->err, scc->list, co)) {
	return;
    }
    gql_eval_sels(scc->err, scc->doc, (gqlRef)&child, &cf, scc->sel->sels, co, scc->depth);
}

// types: [__Type!]!
static int
schema_types(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char		*key = sel->name;
    gqlValue		list = gql_list_create(err, NULL);
    struct _schemaCbCtx	scc = {
	.err = err,
	.doc = doc,
	.sel = sel,
	.depth = depth + 1,
	.list = list,
    };

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    gql_type_iterate(schema_types_cb, &scc);

    return err->code;
}

static int
schema_op_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth, const char *op) {
    const char		*key = sel->name;
    struct _gqlCobj	child = { .clas = &type_class };
    gqlValue		co;
    gqlType		type;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == (type = gql_type_func(gql_root_op(op)))) {
	if (NULL == (co = gql_null_create(err)) ||
	    AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	    return err->code;
	}
	return AGOO_ERR_OK;
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    child.ptr = (void*)type;

    return gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, depth + 1);
}

// queryType: __Type!
static int
schema_query_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    return schema_op_type(err, doc, obj, field, sel, result, depth, "query");
}

// mutationType: __Type
static int
schema_mutation_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    return schema_op_type(err, doc, obj, field, sel, result, depth, "mutation");
}

// subscriptionType: __Type
static int
schema_subscription_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    return schema_op_type(err, doc, obj, field, sel, result, depth, "subscription");
}

// directives: [__Directive!]!
static int
schema_directives(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    const char		*key = sel->name;
    gqlValue		list = gql_list_create(err, NULL);
    gqlValue		co;
    struct _gqlField	cf;
    struct _gqlCobj	child = { .clas = &directive_class };
    int			d2 = depth + 1;
    gqlDir		d = gql_directives;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == list ||
	AGOO_ERR_OK != gql_object_set(err, result, key, list)) {
	return err->code;
    }
    memset(&cf, 0, sizeof(cf));
    cf.type = sel->type->base;

    for (d = gql_directives; NULL != d; d = d->next) {
	if (NULL == (co = gql_object_create(err)) ||
	    AGOO_ERR_OK != gql_list_append(err, list, co)) {
	    return err->code;
	}
	child.ptr = d;
	if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, &cf, sel->sels, co, d2)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static struct _gqlCmethod	schema_methods[] = {
    { .key = "types",            .func = schema_types },
    { .key = "queryType",        .func = schema_query_type },
    { .key = "mutationType",     .func = schema_mutation_type },
    { .key = "subscriptionType", .func = schema_subscription_type },
    { .key = "directives",       .func = schema_directives },
    { .key = NULL,               .func = NULL },
};

static struct _gqlCclass	schema_class = {
    .name = "__Schema",
    .methods = schema_methods,
};

static int
root_schema(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    struct _gqlCobj	child = { .clas = &schema_class, .ptr = NULL };
    gqlValue		co;
    const char		*key = sel->name;

    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    return gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, depth + 1);
}

static int
root_type(agooErr err, gqlDoc doc, gqlCobj obj, gqlField field, gqlSel sel, gqlValue result, int depth) {
    gqlValue		na = gql_extract_arg(err, field, sel, "name");
    const char		*name = NULL;
    const char		*key = sel->name;
    int			d2 = depth + 1;
    gqlType		type;
    gqlValue		co;
    struct _gqlCobj	child = { .clas = &type_class };

    if (NULL != na) {
	name = gql_string_get(na);
    }
    if (NULL == name) {
	return agoo_err_set(err, AGOO_ERR_ARG, "%s field requires a name argument. %s:%d", sel->name, __FILE__, __LINE__);
    }
    if (NULL == (type = gql_type_get(name))) {
	return agoo_err_set(err, AGOO_ERR_ARG, "%s is not a defined type. %s:%d", name, __FILE__, __LINE__);
    }
    if (NULL != sel->alias) {
	key = sel->alias;
    }
    if (NULL == (co = gql_object_create(err)) ||
	AGOO_ERR_OK != gql_object_set(err, result, key, co)) {
	return err->code;
    }
    child.ptr = type;
    if (AGOO_ERR_OK != gql_eval_sels(err, doc, (gqlRef)&child, field, sel->sels, co, d2)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static struct _gqlCmethod	root_methods[] = {
    { .key = "__type",   .func = root_type },
    { .key = "__schema", .func = root_schema },
    { .key = NULL,       .func = NULL },
};

static struct _gqlCclass	root_class = {
    .name = "__Query",
    .methods = root_methods,
};

int
gql_intro_eval(agooErr err, gqlDoc doc, gqlSel sel, gqlValue result, int depth) {
    struct _gqlField	field;
    struct _gqlCobj	obj;

    if (0 == strcmp("__type", sel->name)) {
	if (1 < depth) {
	    return agoo_err_set(err, AGOO_ERR_EVAL, "__type can only be called from a query root.");
	}
	obj.clas = &root_class;
	obj.ptr = NULL;
    } else if (0 == strcmp("__schema", sel->name)) {
	if (1 < depth) {
	    return agoo_err_set(err, AGOO_ERR_EVAL, "__scheme can only be called from a query root.");
	}
	obj.clas = &root_class;
	obj.ptr = NULL;
    } else {
	return agoo_err_set(err, AGOO_ERR_EVAL, "%s can only be called from the query root.", sel->name);
    }
    memset(&field, 0, sizeof(field));
    field.name = sel->name;
    field.type = sel->type;

    doc->funcs.resolve = gql_cobj_resolve;
    doc->funcs.type = gql_cobj_ref_type;

    return gql_cobj_resolve(err, doc, &obj, &field, sel, result, depth);
}
