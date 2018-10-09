// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "gqlvalue.h"
#include "graphql.h"

static int	coerce_int(Err err, gqlValue src, gqlType type);
static int	coerce_i64(Err err, gqlValue src, gqlType type);
static int	coerce_string(Err err, gqlValue src, gqlType type);

static Text	int_to_text(Text text, gqlValue value);
static Text	i64_to_text(Text text, gqlValue value);
static Text	string_to_text(Text text, gqlValue value);

static struct _gqlType	int_type = {
    .name = "Int",
    .desc = "Int scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .coerce = coerce_int,
    .to_text = int_to_text,
};

static struct _gqlType	i64_type = {
    .name = "I64",
    .desc = "64 bit integer scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .coerce = coerce_i64,
    .to_text = i64_to_text,
};

static struct _gqlType	string_type = {
    .name = "String",
    .desc = "String scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .coerce = coerce_string,
    .to_text = string_to_text,
};

static struct _gqlType	str16_type = { // unregistered
    .name = "str16",
    .desc = NULL,
    .kind = GQL_SCALAR,
    .locked = true,
    .coerce = coerce_string,
    .to_text = string_to_text,
};

gqlValue
gql_value_create(Err err) {
    // TBD
    return NULL;
}

void
gql_value_destroy(gqlValue value) {
    // TBD
}

int
gql_value_init(Err err) {
    if (ERR_OK != gql_type_set(err, &int_type) ||
	ERR_OK != gql_type_set(err, &i64_type) ||
	ERR_OK != gql_type_set(err, &string_type)) {
	return err->code;
    }
    return ERR_OK;
}

/// set functions /////////////////////////////////////////////////////////////
void
gql_int_set(gqlValue value, int32_t i) {
    value->type = &int_type;
    value->i = i;
}

void
gql_i64_set(gqlValue value, int64_t i) {
    value->type = &i64_type;
    value->i = i;
}

void
gql_string_set(gqlValue value, const char *str) {
    value->type = &string_type;
    if (NULL == str) {
	value->str = NULL;
    } else {
	size_t	len = strlen(str);

	if (len < sizeof(value->str16)) {
	    value->type = &str16_type;
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
    // TBD
    return NULL;
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

/// coerce functions //////////////////////////////////////////////////////////
static int
coerce_int(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static int
coerce_i64(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

static int
coerce_string(Err err, gqlValue src, gqlType type) {
    // TBD
    return ERR_OK;
}

/// to_text functions /////////////////////////////////////////////////////////
static Text
int_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

static Text
i64_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

static Text
string_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}



Text
gql_value_text(Text text, gqlValue value, int indent) {
    // TBD
    return text;
}

