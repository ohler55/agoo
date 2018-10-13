// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "graphql.h"
#include "gqlintro.h"
#include "gqlvalue.h"

#define BUCKET_SIZE	64
#define BUCKET_MASK	63
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

static uint64_t
calc_hash(const char *name) {
    uint64_t		h = 0;
    const uint8_t	*k = (const uint8_t*)name;
    uint64_t		x;
    
    for (; '\0' != *k; k++) {
	if (0 == (x = name_chars[*k])) {
	    return BAD_NAME;
	}
	h = 37 * h + x; // fast, just spread it out
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
	case GQL_FRAG:
	case GQL_INTERFACE:
	case GQL_INPUT: {
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
	    // TBD INTERFACE, INPUT
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
static struct _gqlType	subscription_type = {
    .name = "Subscription",
    .desc = "The root Subscription type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = gql_object_to_text,
};

static struct _gqlType	mutation_type = {
    .name = "Mutation",
    .desc = "The root Mutation type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = gql_object_to_text,
};

static struct _gqlType	query_type = {
    .name = "Query",
    .desc = "The root Query type.",
    .kind = GQL_OBJECT,
    .locked = true,
    .fields = NULL,
    .interfaces = NULL,
    .to_text = gql_object_to_text,
};

static struct _gqlField	subscription_field = {
    .next = NULL,
    .name = "subscription",
    .desc = "Root level subscription.",
    .type = &subscription_type,
    .required = false,
    .list = false,
    .resolve = NULL,
    .args = NULL,
};

static struct _gqlField	mutation_field = {
    .next = &subscription_field,
    .name = "mutation",
    .desc = "Root level mutation.",
    .type = &mutation_type,
    .required = false,
    .list = false,
    .resolve = NULL,
    .args = NULL,
};

static struct _gqlField	query_field = {
    .next = &mutation_field,
    .name = "query",
    .desc = "Root level query.",
    .type = &query_type,
    .required = true,
    .list = false,
    .resolve = NULL,
    .args = NULL,
};

static struct _gqlType	schema_type = {
    .name = "schema",
    .desc = "The GraphQL root Object.",
    .kind = GQL_OBJECT,
    .locked = true,
    .core = true, // TBD define using type create and set core to false
    .fields = &query_field,
    .interfaces = NULL,
    .to_text = gql_object_to_text,
};

int
gql_init(Err err) {
    memset(buckets, 0, sizeof(buckets));

    if (ERR_OK != gql_value_init(err) ||
	ERR_OK != gql_intro_init(err) ||
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

gqlType
type_create(Err err, const char *name, const char *desc, bool locked) {
    gqlType	type = (gqlType)malloc(sizeof(struct _gqlType));

    if (NULL == type) {
	err_set(err, ERR_MEMORY, "Failed to allocation memory for a GraphQL Type.");
    } else {
	DEBUG_ALLOC(mem_graphql_type, type);
	type->name = strdup(name);
	if (NULL == desc) {
	    type->desc = NULL;
	} else {
	    type->desc = strdup(desc);
	}
	type->locked = locked;
	type->core = false;

	if (ERR_OK != gql_type_set(err, type)) {
	    gql_type_destroy(type);
	    type = NULL;
	}
    }
    return type;
}

Text
gql_object_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_type_create(Err err, const char *name, const char *desc, bool locked, gqlType *interfaces) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_OBJECT;
	type->to_text = gql_object_to_text;
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
gql_type_field(Err err,
	       gqlType type,
	       const char *name,
	       gqlType return_type,
	       const char *desc,
	       bool required,
	       bool list,
	       bool list_required,
	       gqlResolveFunc resolve) {
    // TBD
    return NULL;
}

gqlArg
gql_field_arg(Err err, gqlField field, const char *name, gqlType type, const char *desc, struct _gqlValue *def_value, bool required) {
    // TBD
    return NULL;
}

Text
gql_union_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_union_create(Err err, const char *name, const char *desc, bool locked, gqlType *types) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_UNION;
	type->to_text = gql_union_to_text;
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

Text
gql_enum_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_enum_create(Err err, const char *name, const char *desc, bool locked, const char **choices) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_ENUM;
	type->to_text = gql_enum_to_text;
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
input_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_input_create(Err err, const char *name, const char *desc, bool locked) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_INPUT;
	type->to_text = input_to_text;
	type->fields = NULL;
    }
    return type;
}

static Text
interface_to_text(Text text, gqlValue value) {
    // TBD
    return text;
}

gqlType
gql_interface_create(Err err, const char *name, const char *desc, bool locked) {
    gqlType	type = type_create(err, name, desc, locked);

    if (NULL != type) {
	type->kind = GQL_INTERFACE;
	type->to_text = interface_to_text;
	type->fields = NULL;
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
field_text(Text text, gqlField f) {
    // TBD
    return text;
}

Text
gql_type_text(Text text, gqlType type, bool comments) {
    if (comments && NULL != type->desc) {
	text = text_append(text, "\"\"\"\n", 4);
	text = text_append(text, type->desc, -1);
	text = text_append(text, "\n\"\"\"\n", 5);
    }
    switch (type->kind) {
    case GQL_OBJECT:
    case GQL_FRAG: {
	gqlField	f;

	text = text_append(text, "type ", 5);
	text = text_append(text, type->name, -1);
	// TBD interfaces
	text = text_append(text, " {\n", 3);

	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_text(text, f);
	}
	text = text_append(text, "}\n", 2);
	break;
    }
    case GQL_UNION: {
	text = text_append(text, "union ", 6);
	text = text_append(text, type->name, -1);
	text = text_append(text, " = ", 3);
	// TBD type separated by |
	break;
    }
    case GQL_ENUM: {
	const char	**cp;
	
	text = text_append(text, "enum ", 5);
	text = text_append(text, type->name, -1);
	text = text_append(text, " {\n", 3);
	for (cp = type->choices; NULL != *cp; cp++) {
	    text = text_append(text, "  ", 2);
	    text = text_append(text, *cp, -1);
	    text = text_append(text, "\n", 1);
	}
	text = text_append(text, "}\n", 2);
	break;
    }
    case GQL_SCALAR:
	text = text_append(text, "scalar ", 7);
	text = text_append(text, type->name, -1);
	text = text_append(text, "\n", 1);
	break;
    case GQL_INTERFACE: {
	gqlField	f;

	text = text_append(text, "interface ", 10);
	text = text_append(text, type->name, -1);
	text = text_append(text, " {\n", 3);

	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_text(text, f);
	}
	text = text_append(text, "}\n", 2);
	break;
    }
    case GQL_INPUT: {
	gqlField	f;

	text = text_append(text, "input ", 6);
	text = text_append(text, type->name, -1);
	text = text_append(text, " {\n", 3);

	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_text(text, f);
	}
	text = text_append(text, "}\n", 2);
	break;
    }
    default:
	break;
    }

    return text;
}

static int
type_cmp(const void *v0, const void *v1) {
    gqlType	t0 = *(gqlType*)v0;
    gqlType	t1 = *(gqlType*)v1;

    if (t0->kind == t1->kind) {
	if (0 == strcmp("schema", t0->name)) {
	    return -1;
	}
	if (0 == strcmp("schema", t1->name)) {
	    return 1;
	}
	return strcmp(t0->name, t1->name);
    }
    return t0->kind - t1->kind;
}

Text
gql_schema_text(Text text, bool comments, bool all) {
    Slot	*bucket;
    Slot	s;
    gqlType	type;
    int		i;
    int		cnt = 0;

    for (bucket = buckets, i = 0; i < BUCKET_SIZE; bucket++, i++) {
	for (s = *bucket; NULL != s; s = s->next) {
	    type = s->type;
	    if (!all && '_' == *type->name && '_' == type->name[1]) {
		continue;
	    }
	    cnt++;
	}
    }
    text = text_append(text, "\n", 1);
    {
	gqlType	types[cnt];
	gqlType	*tp = types;
	
	for (bucket = buckets, i = 0; i < BUCKET_SIZE; bucket++, i++) {
	    for (s = *bucket; NULL != s; s = s->next) {
		type = s->type;
		if (!all && '_' == *type->name && '_' == type->name[1]) {
		    continue;
		}
		*tp++ = type;
	    }
	}
	qsort(types, cnt, sizeof(gqlType), type_cmp);
	for (i = 0, tp = types; i < cnt; i++, tp++) {
	    text = gql_type_text(text, *tp, comments);
	    text = text_append(text, "\n", 1);
	}
    }
    return text;
}
