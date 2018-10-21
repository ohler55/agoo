// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gqlvalue.h"
#include "graphql.h"


// Int type
static int
coerce_int(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
int_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_int_type = {
    .name = "Int",
    .desc = "Int scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_int,
    .destroy = NULL,
    .to_text = int_to_text,
};

// I64 type, add on type.
static int
coerce_i64(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
i64_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_i64_type = {
    .name = "I64",
    .desc = "64 bit integer scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_i64,
    .destroy = NULL,
    .to_text = i64_to_text,
};

// String type
static void
string_destroy(gqlValue value) {
    free((char*)value->str);
}

static int
coerce_string(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
string_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_string_type = {
    .name = "String",
    .desc = "String scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_string,
    .destroy = string_destroy,
    .to_text = string_to_text,
};

// Alternative to String but with no destroy needed.
struct _gqlType	gql_str16_type = { // unregistered
    .name = "str16",
    .desc = NULL,
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_string,
    .destroy = NULL,
    .to_text = string_to_text,
};

// Bool type
static int
coerce_bool(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
bool_to_text(Text text, gqlValue value) {
    if (value->b) {
	text = text_append(text, "true", 4);
    } else {
	text = text_append(text, "false", 5);
    }
    return text;
}

struct _gqlType	gql_bool_type = {
    .name = "Bool",
    .desc = "Bool scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_bool,
    .destroy = NULL,
    .to_text = bool_to_text,
};

// Float type
static int
coerce_float(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
float_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_float_type = {
    .name = "Float",
    .desc = "Float scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_float,
    .destroy = NULL,
    .to_text = float_to_text,
};

// Time type
static int
coerce_time(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
time_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_time_type = {
    .name = "Time",
    .desc = "Time zulu scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_time,
    .destroy = NULL,
    .to_text = time_to_text,
};

// Uuid type
static int
coerce_uuid(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
uuid_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_uuid_type = {
    .name = "Uuid",
    .desc = "UUID scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_uuid,
    .destroy = NULL,
    .to_text = uuid_to_text,
};

// Url type
static void
url_destroy(gqlValue value) {
    free((char*)value->url);
}

static int
coerce_url(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static Text
url_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

struct _gqlType	gql_url_type = {
    .name = "Url",
    .desc = "URL scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = coerce_url,
    .destroy = url_destroy,
    .to_text = url_to_text,
};

////////////////////////////////////////////////////////////////////////////////
gqlValue
gql_value_create(Err err) {
    // TBD
    return NULL;
}

void
gql_value_destroy(gqlValue value) {
    if (NULL != value->type->destroy) {
	value->type->destroy(value);
    }
    DEBUG_ALLOC(mem_graphql_value, value);
    free(value);
}

int
gql_value_init(Err err) {
    if (ERR_OK != gql_type_set(err, &gql_int_type) ||
	ERR_OK != gql_type_set(err, &gql_i64_type) ||
	ERR_OK != gql_type_set(err, &gql_bool_type) ||
	ERR_OK != gql_type_set(err, &gql_float_type) ||
	ERR_OK != gql_type_set(err, &gql_time_type) ||
	ERR_OK != gql_type_set(err, &gql_uuid_type) ||
	ERR_OK != gql_type_set(err, &gql_url_type) ||
	ERR_OK != gql_type_set(err, &gql_string_type)) {
	return err->code;
    }
    return ERR_OK;
}

/// set functions /////////////////////////////////////////////////////////////
void
gql_int_set(gqlValue value, int32_t i) {
    value->type = &gql_int_type;
    value->i = i;
}

void
gql_i64_set(gqlValue value, int64_t i) {
    value->type = &gql_i64_type;
    value->i = i;
}

void
gql_string_set(gqlValue value, const char *str) {
    value->type = &gql_string_type;
    if (NULL == str) {
	value->str = NULL;
    } else {
	size_t	len = strlen(str);

	if (len < sizeof(value->str16)) {
	    value->type = &gql_str16_type;
	    strcpy(value->str16, str);
	} else {
	    value->str = strdup(str);
	}
    }
}

int
gql_url_set(Err err, gqlValue value, const char *url) {
    // TBD
    return ERR_OK;
}

void
gql_bool_set(gqlValue value, bool b) {
    // TBD
}

void
gql_float_set(gqlValue value, double f) {
    // TBD
}

void
gql_time_set(gqlValue value, int64_t t) {
    // TBD
}

int
gql_time_str_set(Err err, gqlValue value, const char *str) {
    // TBD
    return ERR_OK;
}

void
gql_uuid_set(gqlValue value, uint64_t hi, uint64_t lo) {
    // TBD
}

int
gql_uuid_str_set(Err err, gqlValue value, const char *str) {
    // TBD
    return ERR_OK;
}

extern void	gql_null_set(gqlValue value);

int
gql_list_append(Err err, gqlValue list, gqlValue item) {
    // TBD
    return ERR_OK;
}

int
gql_object_append(Err err, gqlValue list, const char *key, gqlValue item) {
    // TBD
    return ERR_OK;
}

/// create functions //////////////////////////////////////////////////////////

static gqlValue
value_create(gqlType type) {
    gqlValue	v = (gqlValue)malloc(sizeof(struct _gqlValue));
    
    if (NULL != v) {
	DEBUG_ALLOC(mem_graphql_value, v);
	memset(v, 0, sizeof(struct _gqlValue));
	v->type = type;
    }
    return v;
}

gqlValue
gql_int_create(Err err, int32_t i) {

    // TBD
    return NULL;
}

gqlValue
gql_i64_create(Err err, int64_t i) {
    // TBD
    return NULL;
}

gqlValue
gql_string_create(Err err, const char *str) {
    // TBD
    return NULL;
}

gqlValue
gql_url_create(Err err, const char *url) {
    // TBD
    return NULL;
}

gqlValue
gql_bool_create(Err err, bool b) {
    gqlValue	v = value_create(&gql_bool_type);
    
    if (NULL != v) {
	v->b = b;
    }
    return v;
}

gqlValue
gql_float_create(Err err, double f) {
    // TBD
    return NULL;
}

gqlValue
gql_time_create(Err err, int64_t t) {
    // TBD
    return NULL;
}

gqlValue
gql_time_str_create(Err err, const char *str) {
    // TBD
    return NULL;
}

gqlValue
gql_uuid_create(Err err, uint64_t hi, uint64_t lo) {
    // TBD
    return NULL;
}

gqlValue
gql_uuid_str_create(Err err, const char *str) {
    // TBD
    return NULL;
}

gqlValue
gql_null_create(Err err) {
    // TBD
    return NULL;
}

gqlValue
gql_list_create(Err err) {
    // TBD
    return NULL;
}

gqlValue
gql_object_create(Err err) {
    // TBD
    return NULL;
}

Text
gql_value_text(Text text, gqlValue value) {
    return value->type->to_text(text, value);
}

