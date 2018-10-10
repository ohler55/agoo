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

static Text	object_to_text(Text text, gqlValue value);

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

static void
type_destroy(gqlType type) {
    if (!type->core) {
	free((char*)type->name);
	free((char*)type->desc);
	switch (type->kind) {
	case GQL_OBJECT:
	case GQL_FRAG: {
	    gqlField	f;
	    gqlArg	a;
	
	    while (NULL != (f = type->fields)) {
		type->fields = f->next;
		while (NULL != (a = f->args)) {
		    f->args = a->next;
		    free((char*)a->name);
		    free((char*)a->desc);
		    gql_value_destroy(a->default_value);
		    DEBUG_FREE(mem_graphql_arg, a);
		    free(a);
		}
		free((char*)f->name);
		free((char*)f->desc);
		DEBUG_FREE(mem_graphql_field, f);
		free(f);
	    }
	    free(type->interfaces);
	    break;
	}
	case GQL_UNION:
	    free(type->utypes);
	    break;
	case GQL_ENUM: {
	    char	**cp;
	    
	    for (cp = (char**)type->choices; NULL != *cp; cp++) {
		free(*cp);
	    }
	    free(type->choices);
	    break;
	}
	default:
	    break;
	}
	DEBUG_FREE(mem_graphql_type, type);
	free(type);
    }
}

gqlType
gql_type_get(const char *name) {
    gqlType	type = NULL;
    uint64_t	h = calc_hash(name);

    if (0 < h) {
	Slot	*bucket = get_bucketp(h);
	Slot	s;

	for (s = *bucket; NULL != s; s = s->next) {
	    if (h == s->hash && 0 == strcmp(s->type->name, name)) {
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
    } else {
	Slot	*bucket = get_bucketp(h);
	Slot	s;
    
	for (s = *bucket; NULL != s; s = s->next) {
	    if (h == s->hash && 0 == strcmp(s->type->name, type->name)) {
		type_destroy(s->type);
		s->type = type;
		return ERR_OK;
	    }
	}
	if (NULL == (s = (Slot)malloc(sizeof(struct _Slot)))) {
	    return err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL type.");
	}
	DEBUG_ALLOC(mem_graphql_slot, s);
	s->hash = h;
	s->type = type;
	s->next = *bucket;
	*bucket = s;
    }
    return ERR_OK;
}

static void
type_remove(gqlType type) {
    uint64_t	h = calc_hash(type->name);

    if (0 < h) {
	Slot	*bucket = get_bucketp(h);
	Slot	s;
	Slot	prev = NULL;

	for (s = *bucket; NULL != s; s = s->next) {
	    if (h == s->hash && 0 == strcmp(s->type->name, type->name)) {
		if (NULL == prev) {
		    *bucket = s->next;
		} else {
		    prev->next = s->next;
		}
		DEBUG_FREE(mem_graphql_slot, s);
		free(s);

		break;
	    }
	    prev = s;
	}
    }
}

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
    .next = &directives_field,
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
    .next = &query_field,
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
	Slot	*b = sp;

	*sp = NULL;
	for (s = *b; NULL != s; s = n) {
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

gqlType
type_create(Err err, const char *name, const char *desc, bool locked) {
    gqlType	type = (gqlType)malloc(sizeof(struct _gqlType));

    if (NULL == type) {
	err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL Type.");
    } else {
	DEBUG_ALLOC(mem_graphql_type, type);
	type->name = strdup(name);
	type->desc = strdup(desc);
	type->locked = locked;
	type->core = false;
    }
    return type;
}

gqlType
gql_type_create(Err err, const char *name, const char *desc, bool locked, gqlType *interfaces) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_OBJECT;
	type->to_text = object_to_text;
	type->fields = NULL;
	type->interfaces = NULL;
	if (NULL != interfaces) {
	    gqlType	*tp = interfaces;
	    gqlType	*np;
	    int		cnt = 0;

	    for (; NULL != *tp; tp++) {
		cnt++;
	    }
	    if (0 < cnt) {
		if (NULL == (type->interfaces = (gqlType*)malloc(sizeof(gqlType) * (cnt + 1)))) {
		    err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL type interfaces.");
		    free(type);
		    return NULL;
		}
		for (np = type->interfaces, tp = interfaces; NULL != *tp; np++, tp++) {
		    *np = *tp;
		}
		*np = NULL;
	    }
	}
    }
    return type;
}

gqlField
gql_type_field(Err err, gqlType type, const char *name, gqlType return_type, const char *desc, bool required, bool list) {
    // TBD
    return NULL;
}

gqlArg
gql_field_arg(Err err, gqlField field, const char *name, gqlType type, const char *desc, struct _gqlValue *def_value, bool required) {
    // TBD
    return NULL;
}

static Text
union_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_union_create(Err err, const char *name, const char *desc, bool locked, gqlType *types) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_UNION;
	type->to_text = union_to_text;
	type->utypes = NULL;
	if (NULL != types) {
	    gqlType	*tp = types;
	    gqlType	*np;
	    int		cnt = 0;

	    for (; NULL != *tp; tp++) {
		cnt++;
	    }
	    if (0 < cnt) {
		if (NULL == (type->utypes = (gqlType*)malloc(sizeof(gqlType) * (cnt + 1)))) {
		    err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL Union.");
		    free(type);
		    return NULL;
		}
		for (np = type->utypes, tp = types; NULL != *tp; np++, tp++) {
		    *np = *tp;
		}
		*np = NULL;
	    }
	}
    }
    return type;
}

static Text
enum_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_enum_create(Err err, const char *name, const char *desc, bool locked, const char **choices) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_ENUM;
	type->to_text = enum_to_text;
	type->choices = NULL;
	if (NULL != choices) {
	    const char	**cp = choices;
	    const char	**dp;
	    int		cnt = 0;

	    for (; NULL != *cp; cp++) {
		cnt++;
	    }
	    if (0 < cnt) {
		if (NULL == (type->choices = (const char**)malloc(sizeof(const char*) * (cnt + 1)))) {
		    err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL Enum.");
		    free(type);
		    return NULL;
		}
		for (dp = type->choices, cp = choices; NULL != *cp; dp++, cp++) {
		    *dp = strdup(*cp);
		}
		*dp = NULL;
	    }
	}
    }
    return type;
}

static Text
fragment_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_fragment_create(Err err, const char *name, const char *desc, bool locked, gqlType on) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_FRAG;
	type->to_text = fragment_to_text;
	type->fields = NULL;
	type->on = on;
    }
    return type;
}

static Text
scalar_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_scalar_create(Err err, const char *name, const char *desc, bool locked) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_SCALAR;
	type->to_text = scalar_to_text;
	type->coerce = NULL;
	type->destroy = NULL;
    }
    return type;
}

void
gql_type_destroy(gqlType type) {
    type_destroy(type);
    type_remove(type);
}

Text
gql_type_text(Text text, gqlType value, int indent) {
    // TBD
    return text;
}

