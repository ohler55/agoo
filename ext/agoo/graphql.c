// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "graphql.h"
#include "gqlvalue.h"

#define BUCKET_SIZE	256
#define BUCKET_MASK	255
#define BAD_NAME	(uint64_t)-1

typedef struct _Slot {
    struct _Slot	*next;
    gqlType		type;
    uint64_t		hash;
} *Slot;

static Slot	buckets[BUCKET_SIZE];

static uint8_t	name_chars[256] = "\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x00\x00\x00\x00\x00\x00\
\x00\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\
\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x00\x00\x00\x00\x25\
\x00\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\
\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\
";


static int	coerce_int(Err err, gqlValue src, gqlType type);

static Text	object_to_text(Text text, gqlValue value);
static Text	int_to_text(Text text, gqlValue value);

static uint64_t
calc_hash(const char *name) {
    uint64_t		h = 0;
    const uint8_t	*k = (const uint8_t*)name;
    uint64_t		x;
    
    for (; '\0' != *k; k++) {
	if (0 == (x = name_chars[*k])) {
	    return BAD_NAME;
	}
	h = 17 * h + x; // fast, just spread it out
    }
    return h;
}

static Slot*
get_bucketp(uint64_t h) {
    return buckets + (BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

gqlType
gql_type_get(const char *name) {
    gqlType	type = NULL;
    uint64_t	h = calc_hash(name);

    if (0 < h) {
	Slot	*bucket = get_bucketp(h);
	Slot	s;

	for (s = *bucket; NULL != s; s = s->next) {
	    if (h == (int64_t)s->hash && 0 == strcmp(s->type->name, name)) {
		type = s->type;
		break;
	    }
	}
    }
    return type;
}

int
gql_type_set(Err err, gqlType type) {
    uint64_t	h = calc_hash(type->name);

    if (h <= 0) {
	return err_set(err, ERR_ARG, "%s is not a valid GraphQL type name.", type->name);
    }
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && 0 == strcmp(s->type->name, type->name)) {
	    gql_type_destroy(s->type);
	    s->type = type;
	    return ERR_OK;
	}
    }
    if (NULL == (s = (Slot)malloc(sizeof(struct _Slot)))) {
	return err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL type.");
    }
    DEBUG_ALLOC(mem_graphql_slot, s)
    s->hash = h;
    s->type = type;
    s->next = *bucket;
    *bucket = s;

    return ERR_OK;
}





#if 0
// Scalar types
struct _gqlType	gql_int = {
    .name = "Int",
    .desc = "Int scalar.",
    .kind = GQL_INT,
    .locked = true,
};

struct _gqlType	gql_string = {
    .name = "String",
    .desc = "String scalar.",
    .kind = GQL_STRING,
    .locked = true,
};

struct _gqlType	gql_float = {
    .name = "Float",
    .desc = "Float scalar.",
    .kind = GQL_FLOAT,
    .locked = true,
};

struct _gqlType	gql_boolean = {
    .name = "Boolean",
    .desc = "Boolean scalar.",
    .kind = GQL_BOOL,
    .locked = true,
};

struct _gqlType	gql_id = {
    .name = "ID",
    .desc = "ID scalar.",
    .kind = GQL_ID,
    .locked = true,
};

struct _gqlType	gql_time = {
    .name = "Time",
    .desc = "Time scalar.",
    .kind = GQL_TIME,
    .locked = true,
};

struct _gqlType	gql_url = {
    .name = "Url",
    .desc = "Url scalar.",
    .kind = GQL_URL,
    .locked = true,
};

struct _gqlType	gql_uuid = {
    .name = "Uuid",
    .desc = "Uuid scalar.",
    .kind = GQL_UUID,
    .locked = true,
};

#endif

// Second level objects.
static struct _gqlType	type_type = {
    .name = "__Type",
    .desc = "The root Type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = object_to_text,
};

static struct _gqlType	query_type = {
    .name = "Query",
    .desc = "The root Query type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = object_to_text,
};

static struct _gqlType	mutation_type = {
    .name = "Mutation",
    .desc = "The root Mutation type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = object_to_text,
};

static struct _gqlType	subscription_type = {
    .name = "Subscription",
    .desc = "The root Subscription type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = object_to_text,
};

static struct _gqlType	directive_type = {
    .name = "__Directive",
    .desc = "The root Directive type.",
    .kind = GQL_OBJECT, // TBD scalar?
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = object_to_text,
};

static struct _gqlField	directives_field = {
    .next = NULL,
    .name = "directives",
    .desc = "Root level directives.",
    .type = &directive_type,
    .required = true,
    .list = true,
    .operation = false,
    .args = NULL,
};

static struct _gqlField	subscription_field = {
    .next = NULL,
    .name = "subscriptionType",
    .desc = "Root level subscription.",
    .type = &type_type,
    .required = false,
    .list = false,
    .operation = false,
    .args = NULL,
};

static struct _gqlField	mutation_field = {
    .next = &subscription_field,
    .name = "mutationType",
    .desc = "Root level mutation.",
    .type = &type_type,
    .required = false,
    .list = false,
    .operation = false,
    .args = NULL,
};

static struct _gqlField	query_field = {
    .next = &mutation_field,
    .name = "queryType",
    .desc = "Root level query.",
    .type = &type_type,
    .required = true,
    .list = false,
    .operation = false,
    .args = NULL,
};

static struct _gqlField	types_field = {
    .next = NULL,
    .name = "types",
    .desc = "Root level subscription.",
    .type = &type_type,
    .required = true,
    .list = true,
    .operation = false,
    .args = NULL,
};

static struct _gqlType	schema_type = {
    .name = "__Schema",
    .desc = "The GraphQL root Object.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = &types_field,
    .interfaces = NULL,
    .to_text = object_to_text,
};

// TBD gqlValue for schema root
//  set fields with functions on the __Schema type
//  on introspect call those functions to build response


int
gql_init(Err err) {
    memset(buckets, 0, sizeof(buckets));

    if (ERR_OK != gql_value_init(err) ||
	ERR_OK != gql_type_set(err, &type_type) ||
	ERR_OK != gql_type_set(err, &query_type) ||
	ERR_OK != gql_type_set(err, &mutation_type) ||
	ERR_OK != gql_type_set(err, &subscription_type) ||
	ERR_OK != gql_type_set(err, &schema_type)) {
	return err->code;
    }
    return ERR_OK;
}

void
gql_destroy() {
    Slot	*sp = buckets;
    Slot	s;
    Slot	n;
    int		i;

    for (i = BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    DEBUG_FREE(mem_graphql_slot, s);
	    gql_type_destroy(s->type);
	    free(s);
	}
	*sp = NULL;
    }
}

static Text
object_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

int
gql_field_create(Err err, const char *name, gqlType type, const char *desc, bool required, bool list) {
    // TBD
    return ERR_OK;
}

int
gql_arg_create(Err err, const char *name, gqlType type, const char *desc, struct _gqlValue *def_value, gqlArg next) {
    // TBD
    return ERR_OK;
}

int
gql_op_create(Err err, const char *name, gqlType return_type, const char *desc, gqlArg args) {
    // TBD
    return ERR_OK;
}

int
gql_type_create(Err err, const char *name, const char *desc, bool locked, gqlField fields, gqlType interfaces) {
    // TBD
    return ERR_OK;
}

int
gql_union_create(Err err, const char *name, const char *desc, bool locked, gqlType types) {
    // TBD
    return ERR_OK;
}

int
gql_enum_create(Err err, const char *name, const char *desc, bool locked, const char **choices) {
    // TBD
    return ERR_OK;
}

int
gql_fragment_create(Err err, const char *name, const char *desc, bool locked, gqlField fields) {
    // TBD
    return ERR_OK;
}

int
gql_scalar_create(Err err, const char *name, const char *desc, bool locked) {
    // TBD
    return ERR_OK;
}

void
gql_type_destroy(gqlType type) {
    // TBD
}

Text
gql_type_text(Text text, gqlType value, int indent) {
    // TBD
    return text;
}

