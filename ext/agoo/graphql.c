// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "graphql.h"
#include "gqlintro.h"
#include "gqlvalue.h"
#include "req.h"
#include "res.h"

#define BUCKET_SIZE	64
#define BUCKET_MASK	63
#define BAD_NAME	(uint64_t)-1

typedef struct _slot {
    struct _slot	*next;
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

static const char	spaces[16] = "                ";

static gqlType	schema_type = NULL;
static gqlDir	directives = NULL;
static bool	inited = false;

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

static const char*
kind_string(gqlKind kind) {
    switch (kind) {
    case GQL_UNDEF:	return "undefined";
    case GQL_OBJECT:	return "object";
    case GQL_INPUT:	return "input";
    case GQL_UNION:	return "union";
    case GQL_INTERFACE:	return "interface";
    case GQL_ENUM:	return "enum";
    case GQL_SCALAR:	return "scalar";
    case GQL_LIST:	return "list";
    default:		break;
    }
    return "unknown";
}

static char*
alloc_string(agooErr err, const char *s, size_t len) {
    char	*a = NULL;
    
    if (NULL != s) {
	if (0 >= len) {
	    if (NULL == (a = strdup(s))) {
		agoo_err_set(err, AGOO_ERR_MEMORY, "strdup() failed.");
	    }
	    len = strlen(a);
	} else {
	    if (NULL == (a = strndup(s, len))) {
		agoo_err_set(err, AGOO_ERR_MEMORY, "strndup() of length %d failed.", len);
	    }
	}
    }
    if (NULL != a) {
	AGOO_ALLOC(a, len);
    }
    return a;
}

static void
free_dir_uses(gqlDirUse uses) {
    gqlDirUse	u;
    gqlLink	link;
    
    while (NULL != (u = uses)) {
	uses = u->next;
	while (NULL != (link = u->args)) {
	    u->args = link->next;
	    gql_link_destroy(link);
	}
	AGOO_FREE(u);
    }
}

static void
arg_destroy(gqlArg a) {
    AGOO_FREE((char*)a->name);
    AGOO_FREE((char*)a->desc);
    if (NULL != a->default_value) {
	gql_value_destroy(a->default_value);
    }
    free_dir_uses(a->dir);

    AGOO_FREE(a);
}

static void
field_destroy(gqlField f) {
    gqlArg	a;
    
    AGOO_FREE((char*)f->name);
    AGOO_FREE((char*)f->desc);

    while (NULL != (a = f->args)) {
	f->args = a->next;
	arg_destroy(a);
    }
    free_dir_uses(f->dir);
    if (NULL != f->default_value) {
	gql_value_destroy(f->default_value);
    }
    AGOO_FREE(f);
}

static void
type_clean(gqlType type) {
    AGOO_FREE((char*)type->desc);
    free_dir_uses(type->dir);

    switch (type->kind) {
    case GQL_OBJECT:
    case GQL_INTERFACE:
    case GQL_INPUT: {
	gqlField	f;
	
	while (NULL != (f = type->fields)) {
	    type->fields = f->next;
	    field_destroy(f);
	}
	AGOO_FREE(type->interfaces);
	break;
    }
    case GQL_UNION: {
	gqlTypeLink	link;

	while (NULL != (link = type->types)) {
	    type->types = link->next;
	    AGOO_FREE(link);
	}
	break;
    }
    case GQL_ENUM: {
	gqlEnumVal	ev;

	while (NULL != (ev = type->choices)) {
	    type->choices = ev->next;
	    AGOO_FREE((char*)ev->value);
	    AGOO_FREE((char*)ev->desc);
	    free_dir_uses(ev->dir);
	    AGOO_FREE(ev);
	}
	break;
    }
    default:
	break;
    }
}

static void
type_destroy(gqlType type) {
    if (&gql_int_type == type ||
	&gql_i64_type == type ||
	&gql_bool_type == type ||
	&gql_float_type == type ||
	&gql_time_type == type ||
	&gql_uuid_type == type ||
	&gql_url_type == type ||
	&gql_string_type == type) {

	return;
    }
    type_clean(type);
    AGOO_FREE((char*)type->name);
    AGOO_FREE(type);
}

static void
dir_clean(gqlDir dir) {
    gqlArg	a;
    gqlStrLink	link;
    
    AGOO_FREE((char*)dir->desc);
    while (NULL != (a = dir->args)) {
	dir->args = a->next;
	arg_destroy(a);
    }
    while (NULL != (link = dir->locs)) {
	dir->locs = link->next;
	AGOO_FREE(link->str);
	AGOO_FREE(link);
    }
}

static void
dir_destroy(gqlDir dir) {
    AGOO_FREE((char*)dir->name);
    dir_clean(dir);
    AGOO_FREE(dir);
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

gqlDir
gql_directive_get(const char *name) {
    gqlDir	dir = directives;

    for (; NULL != dir; dir = dir->next) {
	if (0 == strcmp(name, dir->name)) {
	    break;
	}
    }
    return dir;
}

int
gql_type_set(agooErr err, gqlType type) {
    uint64_t	h = calc_hash(type->name);

    if (h <= 0) {
	return agoo_err_set(err, AGOO_ERR_ARG, "%s is not a valid GraphQL type name.", type->name);
    } else {
	Slot	*bucket = get_bucketp(h);
	Slot	s;
    
	for (s = *bucket; NULL != s; s = s->next) {
	    if (h == s->hash && 0 == strcmp(s->type->name, type->name)) {
		type_destroy(s->type);
		s->type = type;
		return AGOO_ERR_OK;
	    }
	}
	if (NULL == (s = (Slot)AGOO_MALLOC(sizeof(struct _slot)))) {
	    return agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL type.");
	}
	s->hash = h;
	s->type = type;
	s->next = *bucket;
	*bucket = s;
    }
    return AGOO_ERR_OK;
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
		AGOO_FREE(s);

		break;
	    }
	    prev = s;
	}
    }
}

int
gql_init(agooErr err) {
    gqlType	query_type;
    gqlType	mutation_type;
    gqlType	subscription_type;

    if (inited) {
	return AGOO_ERR_OK;
    }
    memset(buckets, 0, sizeof(buckets));
    if (AGOO_ERR_OK != gql_value_init(err) ||
	AGOO_ERR_OK != gql_intro_init(err)) {

	return err->code;
    }
    if (NULL == (query_type = gql_type_create(err, "Query", "The GraphQL root Query.", 0, NULL)) ||
	NULL == (mutation_type = gql_type_create(err, "Mutation", "The GraphQL root Mutation.", 0, NULL)) ||
	NULL == (subscription_type = gql_type_create(err, "Subscription", "The GraphQL root Subscription.", 0, NULL)) ||
	NULL == (schema_type = gql_type_create(err, "schema", "The GraphQL root Object.", 0, NULL)) ||

	NULL == gql_type_field(err, schema_type, "query", query_type, NULL, "Root level query.", 0, false, NULL) ||
	NULL == gql_type_field(err, schema_type, "mutation", mutation_type, NULL, "Root level mutation.", 0, false, NULL) ||
	NULL == gql_type_field(err, schema_type, "subscription", subscription_type, NULL, "Root level subscription.", 0, false, NULL)) {

	return err->code;
    }
    inited = true;
    
    return AGOO_ERR_OK;
}

void
gql_destroy() {
    Slot	*sp = buckets;
    Slot	s;
    Slot	n;
    int		i;
    gqlDir	dir;

    for (i = BUCKET_SIZE; 0 < i; i--, sp++) {
	s = *sp;
	
	*sp = NULL;
	for (; NULL != s; s = n) {
	    n = s->next;
	    type_destroy(s->type);
	    AGOO_FREE(s);
	}
    }
    while (NULL != (dir = directives)) {
	directives = dir->next;
	dir_destroy(dir);
    }
    inited = false;
}

static gqlType
type_create(agooErr err, gqlKind kind, const char *name, const char *desc, size_t dlen) {
    gqlType	type = gql_type_get(name);

    if (NULL == type) {
	if (NULL == (type = (gqlType)AGOO_MALLOC(sizeof(struct _gqlType)))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Type.");
	    return NULL;
	}
	if (NULL == (type->name = strdup(name))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of type name failed. %s:%d", __FILE__, __LINE__);
	    return NULL;
	}
	AGOO_ALLOC(type->name, strlen(type->name));
	if (NULL == (type->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
	type->to_sdl = NULL;
	type->dir = NULL;
	type->kind = kind;
	type->core = false;
	type->to_json = gql_object_to_json;
	type->to_sdl = gql_object_to_sdl;
	
	if (AGOO_ERR_OK != gql_type_set(err, type)) {
	    gql_type_destroy(type);
	    type = NULL;
	}
    } else if (GQL_UNDEF == type->kind) {
	if (NULL == (type->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
	type->kind = kind;
    } else if (type->core) {
	agoo_err_set(err, AGOO_ERR_LOCK, "%d can not be modified.", name);
	return NULL;
    } else if (kind == type->kind) { // looks like it is being modified so remove all the old stuff
	type_clean(type);
    } else {
	agoo_err_set(err, AGOO_ERR_LOCK, "%d already exists as a %s.", name, kind_string(type->kind));
	return NULL;
    }
    return type;
}

static gqlType
type_undef_create(agooErr err, const char *name) {
    return type_create(err, GQL_UNDEF, name, NULL, 0);
}

agooText
gql_object_to_graphql(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_assure_type(agooErr err, const char *name) {
    gqlType	type = NULL;
    
    if (NULL != name) {
	type = gql_type_get(name);

	if (NULL == type) {
	    if (NULL == (type = type_undef_create(err, name))) {
		return NULL;
	    }
	}
    }
    return type;
}

static gqlDir
assure_directive(agooErr err, const char *name) {
    gqlDir	dir = gql_directive_get(name);

    if (NULL == dir) {
	if (NULL == (dir = (gqlDir)AGOO_MALLOC(sizeof(struct _gqlDir)))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive.");
	    return NULL;
	}
	dir->next = directives;
	directives = dir;
	dir->name = strdup(name);
	AGOO_ALLOC(dir->name, strlen(dir->name));
	dir->args = NULL;
	dir->locs = NULL;
	dir->defined = false;
	dir->desc = NULL;
    }
    return dir;
}

gqlType
gql_type_create(agooErr err, const char *name, const char *desc, size_t dlen, gqlTypeLink interfaces) {
    gqlType	type = type_create(err, GQL_OBJECT, name, desc, dlen);

    if (NULL != type) {
	type->to_json = gql_object_to_json;
	type->to_sdl = gql_object_to_sdl;
	type->fields = NULL;
	type->interfaces = interfaces;
    }
    return type;
}

gqlField
gql_type_field(agooErr		err,
	       gqlType		type,
	       const char	*name,
	       gqlType		return_type,
	       gqlValue		default_value,
	       const char	*desc,
	       size_t		dlen,
	       bool		required,
	       gqlResolveFunc	resolve) {
    gqlField	f = (gqlField)AGOO_MALLOC(sizeof(struct _gqlField));
    
    if (NULL == f) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL field.");
    } else {
	f->next = NULL;
	f->name = strdup(name);
	AGOO_ALLOC(f->name, strlen(f->name));
	f->type = return_type;
	if (NULL == (f->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
	f->args = NULL;
	f->dir = NULL;
	f->default_value = default_value;
	f->resolve = resolve;
	f->required = required;
	if (NULL == type->fields) {
	    type->fields = f;
	} else {
	    gqlField	fend;

	    for (fend = type->fields; NULL != fend->next; fend = fend->next) {
	    }
	    fend->next = f;
	}
    }
    return f;
}

gqlArg
gql_field_arg(agooErr		err,
	      gqlField		field,
	      const char	*name,
	      gqlType		type,
	      const char	*desc,
	      size_t		dlen,
	      struct _gqlValue	*def_value,
	      bool		required) {
    gqlArg	a = (gqlArg)AGOO_MALLOC(sizeof(struct _gqlArg));
    
    if (NULL == a) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL field argument.");
    } else {
	a->next = NULL;
	a->name = strdup(name);
	AGOO_ALLOC(a->name, strlen(a->name));
	a->type = type;
	if (NULL == (a->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
	a->default_value = def_value;
	a->dir = NULL;
	a->required = required;
	if (NULL == field->args) {
	    field->args = a;
	} else {
	    gqlArg	end;

	    for (end = field->args; NULL != end->next; end = end->next) {
	    }
	    end->next = a;
	}
    }
    return a;
}

agooText
gql_union_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

agooText
gql_union_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_union_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_UNION, name, desc, dlen);

    if (NULL != type) {
	type->to_json = gql_union_to_json;
	type->to_sdl = gql_union_to_sdl;
	type->types = NULL;
    }
    return type;
}

int
gql_union_add(agooErr err, gqlType type, gqlType member) {
    gqlTypeLink	link = (gqlTypeLink)AGOO_MALLOC(sizeof(struct _gqlTypeLink));

    if (NULL == link) {
	return agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Union value.");
    }
    link->type = member;
    if (NULL == type->types) {
	link->next = type->types;
	type->types = link;
    } else {
	gqlTypeLink	last = type->types;

	for (; NULL != last->next; last = last->next) {
	}
	link->next = NULL;
	last->next = link;
    }
    return AGOO_ERR_OK;
}

agooText
gql_enum_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

agooText
gql_enum_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_enum_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_ENUM, name, desc, dlen);

    if (NULL != type) {
	type->to_json = gql_enum_to_json;
	type->to_sdl = gql_enum_to_sdl;
	type->choices = NULL;
    }
    return type;
}

gqlEnumVal
gql_enum_append(agooErr err, gqlType type, const char *value, size_t len, const char *desc, size_t dlen) {
    gqlEnumVal	ev = (gqlEnumVal)AGOO_MALLOC(sizeof(struct _gqlEnumVal));

    if (NULL == ev) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Enum value.");
	return NULL;
    }
    if (0 >= len) {
	len = strlen(value);
    }
    if (NULL == (ev->value = strndup(value, len))) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "strdup or strndup of length %d failed. %s:%d", len, __FILE__, __LINE__);
	return NULL;
    }
    AGOO_ALLOC(ev->value, len);
    ev->dir = NULL;
    if (NULL == (ev->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	return NULL;
    }
    if (NULL == type->choices) {
	ev->next = type->choices;
	type->choices = ev;
    } else {
	gqlEnumVal	last = type->choices;

	for (; NULL != last->next; last = last->next) {
	}
	ev->next = NULL;
	last->next = ev;
    }
    return ev;
}

static agooText
input_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

static agooText
input_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_input_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_INPUT, name, desc, dlen);

    if (NULL != type) {
	type->to_json = input_to_json;
	type->to_sdl = input_to_sdl;
	type->fields = NULL;
    }
    return type;
}

static agooText
interface_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

static agooText
interface_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_interface_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_INTERFACE, name, desc, dlen);

    if (NULL != type) {
	type->to_json = interface_to_json;
	type->to_sdl = interface_to_sdl;
	type->fields = NULL;
    }
    return type;
}

static agooText
scalar_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (NULL == value->str) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append(text, value->str, -1);
	text = agoo_text_append(text, "\"", 1);
    }
    return text;
}

// Create a scalar type that will be represented as a string.
gqlType
gql_scalar_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_SCALAR, name, desc, dlen);

    if (NULL != type) {
	type->to_json = scalar_to_text;
	type->to_sdl = scalar_to_text;
	type->coerce = NULL;
	type->destroy = NULL;
    }
    return type;
}

gqlDir
gql_directive_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlDir	dir = gql_directive_get(name);

    if (NULL != dir) {
	if (!dir->defined) {
	    dir->defined = true;
	    if (NULL == (dir->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
		return NULL;
	    }
	} else {
	    dir_clean(dir);
	    dir->args = NULL;
	    dir->locs = NULL;
	    dir->defined = true;
	    if (NULL == (dir->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
		return NULL;
	    }
	}
    } else {
	if (NULL == (dir = (gqlDir)AGOO_MALLOC(sizeof(struct _gqlDir)))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive.");
	    return NULL;
	}
	dir->next = directives;
	directives = dir;
	dir->name = strdup(name);
	AGOO_ALLOC(dir->name, strlen(dir->name));
	dir->args = NULL;
	dir->locs = NULL;
	dir->defined = true;
	dir->core = false;
	if (NULL == (dir->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
    }
    return dir;
}

gqlArg
gql_dir_arg(agooErr 		err,
	    gqlDir 		dir,
	    const char 		*name,
	    gqlType		type,
	    const char	 	*desc,
	    size_t		dlen,
	    struct _gqlValue	*def_value,
	    bool 		required) {

    gqlArg	a = (gqlArg)AGOO_MALLOC(sizeof(struct _gqlArg));
    
    if (NULL == a) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive argument.");
    } else {
	a->next = NULL;
	a->name = strdup(name);
	AGOO_ALLOC(a->name, strlen(a->name));
	a->type = type;
	if (NULL == (a->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    return NULL;
	}
	a->default_value = def_value;
	a->dir = NULL;
	a->required = required;
	if (NULL == dir->args) {
	    dir->args = a;
	} else {
	    gqlArg	end;

	    for (end = dir->args; NULL != end->next; end = end->next) {
	    }
	    end->next = a;
	}
    }
    return a;
}

int
gql_directive_on(agooErr err, gqlDir d, const char *on, int len) {
    gqlStrLink	link = (gqlStrLink)AGOO_MALLOC(sizeof(struct _gqlStrLink));
    gqlStrLink	loc;

    if (NULL == link) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive location.");
    }
    if (0 >= len) {
	len = (int)strlen(on);
    }
    if (NULL == d->locs) {
	link->next = d->locs;
	d->locs = link;
    } else {
	link->next = NULL;
	for (loc = d->locs; NULL != loc->next; loc = loc->next) {
	}
	loc->next = link;
    }
    if (NULL == (link->str = strndup(on, len))) {
	return agoo_err_set(err, AGOO_ERR_MEMORY, "strdup or strndup of length %d failed. %s:%d", len, __FILE__, __LINE__);
    }
    AGOO_ALLOC(link->str, len);

    return AGOO_ERR_OK;
}

gqlType
gql_assure_list(agooErr err, gqlType base, bool not_empty) {
    char	name[256];
    gqlType	type;

    if ((int)sizeof(name) <= snprintf(name, sizeof(name), "[%s%s]", base->name, not_empty ? "!" : "")) {
	agoo_err_set(err, AGOO_ERR_ARG, "Name too long");
	return NULL;
    }
    if (NULL == (type = gql_type_get(name))) {
	if (NULL == (type = type_create(err, GQL_LIST, name, NULL, 0))) {
	    return NULL;
	}
	type->base = base;
	type->not_empty = not_empty;
    }
    return type;
}

void
gql_type_destroy(gqlType type) {
    type_destroy(type);
    type_remove(type);
}

// If negative then there are non-simple-string characters.
static int
desc_len(const char *desc) {
    const char	*d = desc;
    int		special = 1;

    for (; '\0' != *d; d++) {
	if (*d < ' ' || '"' == *d || '\\' == *d) {
	    special = -1;
	}
    }
    return (int)(d - desc) * special;
}

static agooText
desc_sdl(agooText text, const char *desc, int indent) {
    if (NULL != desc) {
	int	cnt = desc_len(desc);

	if (0 < indent) {
	    text = agoo_text_append(text, spaces, indent);
	}
	if (0 <= cnt) {
	    text = agoo_text_append(text, "\"", 1);
	    text = agoo_text_append(text, desc, cnt);
	    text = agoo_text_append(text, "\"\n", 2);
	} else {
	    text = agoo_text_append(text, "\"\"\"\n", 4);
	    if (0 < indent) {
		const char	*start = desc;
		const char	*d = desc;
	    
		for (; '\0' != *d; d++) {
		    if ('\r' == *d) {
			int	len = (int)(d - start);
			d++;
			if ('\n' == *d) {
			    d++;
			}
			text = agoo_text_append(text, spaces, indent);
			text = agoo_text_append(text, start, len);
			text = agoo_text_append(text, "\n", 1);
			start = d;
		    } else if ('\n' == *d) {
			text = agoo_text_append(text, spaces, indent);
			text = agoo_text_append(text, start, (int)(d - start));
			text = agoo_text_append(text, "\n", 1);
			d++;
			start = d;
		    }
		}
		text = agoo_text_append(text, spaces, indent);
		text = agoo_text_append(text, start, (int)(d - start));
	    } else {
		text = agoo_text_append(text, desc, -1);
	    }
	    if (0 < indent) {
		text = agoo_text_append(text, "\n", 1);
		text = agoo_text_append(text, spaces, indent);
		text = agoo_text_append(text, "\"\"\"\n", 4);
	    } else {
		text = agoo_text_append(text, "\n\"\"\"\n", 5);
	    }
	}
    }
    return text;
}

static agooText
arg_sdl(agooText text, gqlArg a, bool with_desc, bool last) {
    if (with_desc) {
	text = desc_sdl(text, a->desc, 4);
    }
    text = agoo_text_append(text, a->name, -1);
    text = agoo_text_append(text, ": ", 2);
    text = agoo_text_append(text, a->type->name, -1);
    if (a->required) {
	text = agoo_text_append(text, "!", 1);
    }
    if (NULL != a->default_value) {
	text = agoo_text_append(text, " = ", 3);
	text = gql_value_sdl(text, a->default_value, 0, 0);
    }
    if (!last) {
	text = agoo_text_append(text, ", ", 2);
    }
    return text;
}

static agooText
append_dir_use(agooText text, gqlDirUse use) {
    for (; NULL != use; use = use->next) {
	text = agoo_text_append(text, " @", 2);
	text = agoo_text_append(text, use->dir->name, -1);
	if (NULL != use->args) {
	    gqlLink	link;
	    
	    text = agoo_text_append(text, "(", 1);
	    for (link = use->args; NULL != link; link = link->next) {
		text = agoo_text_append(text, link->key, -1);
		text = agoo_text_append(text, ": ", 2);
		text = gql_value_sdl(text, link->value, 0, 0);
	    }
	    text = agoo_text_append(text, ")", 1);
	}
    }
    return text;
}

static agooText
field_sdl(agooText text, gqlField f, bool with_desc) {
    if (with_desc) {
	text = desc_sdl(text, f->desc, 2);
    }
    text = agoo_text_append(text, "  ", 2);
    text = agoo_text_append(text, f->name, -1);
    if (NULL != f->args) {
	gqlArg	a;

	text = agoo_text_append(text, "(", 1);
	for (a = f->args; NULL != a; a = a->next) {
	    text = arg_sdl(text, a, with_desc, NULL == a->next);
	}
	text = agoo_text_append(text, ")", 1);
    }
    text = agoo_text_append(text, ": ", 2);
    text = agoo_text_append(text, f->type->name, -1);
    if (f->required) {
	text = agoo_text_append(text, "!", 1);
    }
    if (NULL != f->default_value) {
	text = agoo_text_append(text, " = ", 3);
	text = gql_value_sdl(text, f->default_value, 0, 0);
    }
    text = append_dir_use(text, f->dir);
    text = agoo_text_append(text, "\n", 1);

    return text;
}

agooText
gql_type_sdl(agooText text, gqlType type, bool with_desc) {
    if (with_desc) {
	desc_sdl(text, type->desc, 0);
    }
    switch (type->kind) {
    case GQL_OBJECT:
    case GQL_FRAG: {
	gqlField	f;

	text = agoo_text_append(text, "type ", 5);
	text = agoo_text_append(text, type->name, -1);
	if (NULL != type->interfaces) {
	    gqlTypeLink	tp = type->interfaces;

	    text = agoo_text_append(text, " implements ", 12);
	    for (; NULL != tp; tp = tp->next) {
		text = agoo_text_append(text, tp->type->name, -1);
		if (NULL != tp->next) {
		    text = agoo_text_append(text, " & ", 3);
		}
	    }
	}
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);
	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_sdl(text, f, with_desc);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
    case GQL_UNION: {
	gqlTypeLink	link;

	text = agoo_text_append(text, "union ", 6);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " = ", 3);
	for (link = type->types; NULL != link; link = link->next) {
	    text = agoo_text_append(text, link->type->name, -1);
	    if (NULL != link->next) {
		text = agoo_text_append(text, " | ", 3);
	    }
	}
	break;
    }
    case GQL_ENUM: {
	gqlEnumVal	ev;
	
	text = agoo_text_append(text, "enum ", 5);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);
	for (ev = type->choices; NULL != ev; ev = ev->next) {
	    text = agoo_text_append(text, "  ", 2);
	    if (with_desc) {
		text = desc_sdl(text, ev->desc, 2);
	    }
	    text = agoo_text_append(text, ev->value, -1);
	    text = append_dir_use(text, ev->dir);
	    text = agoo_text_append(text, "\n", 1);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
    case GQL_SCALAR:
	text = agoo_text_append(text, "scalar ", 7);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, "\n", 1);
	break;
    case GQL_INTERFACE: {
	gqlField	f;

	text = agoo_text_append(text, "interface ", 10);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);

	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_sdl(text, f, with_desc);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
    case GQL_INPUT: {
	gqlField	f;

	text = agoo_text_append(text, "input ", 6);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);
	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_sdl(text, f, with_desc);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
    default:
	break;
    }
    return text;
}

agooText
gql_directive_sdl(agooText text, gqlDir d, bool with_desc) {
    gqlStrLink	link;
    
    if (with_desc) {
	text = desc_sdl(text, d->desc, 0);
    }
    text = agoo_text_append(text, "directive @", 11);
    text = agoo_text_append(text, d->name, -1);
    if (NULL != d->args) {
	gqlArg	a;

	text = agoo_text_append(text, "(", 1);
	for (a = d->args; NULL != a; a = a->next) {
	    text = arg_sdl(text, a, with_desc, NULL == a->next);
	}
	text = agoo_text_append(text, ")", 1);
    }
    text = agoo_text_append(text, " on ", 4);
    for (link = d->locs; NULL != link; link = link->next) {
	text = agoo_text_append(text, link->str, -1);
	if (NULL != link->next) {
	    text = agoo_text_append(text, " | ", 3);
	}
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

agooText
gql_schema_sdl(agooText text, bool with_desc, bool all) {
    Slot	*bucket;
    Slot	s;
    gqlType	type;
    gqlDir	d;
    int		i;
    int		cnt = 0;

    for (bucket = buckets, i = 0; i < BUCKET_SIZE; bucket++, i++) {
	for (s = *bucket; NULL != s; s = s->next) {
	    type = s->type;
	    if (GQL_LIST == type->kind) {
		continue;
	    }
	    if (!all && type->core) {
		continue;
	    }
	    cnt++;
	}
    }
    if (0 < cnt) {
	gqlType	types[cnt];
	gqlType	*tp = types;
	
	for (bucket = buckets, i = 0; i < BUCKET_SIZE; bucket++, i++) {
	    for (s = *bucket; NULL != s; s = s->next) {
		type = s->type;
		if (GQL_LIST == type->kind) {
		    continue;
		}
		if (!all && type->core) {
		    continue;
		}
		*tp++ = type;
	    }
	}
	qsort(types, cnt, sizeof(gqlType), type_cmp);
	for (i = 0, tp = types; i < cnt; i++, tp++) {
	    text = gql_type_sdl(text, *tp, with_desc);
	    if (i < cnt - 1) {
		text = agoo_text_append(text, "\n", 1);
	    }
	}
    }
    for (d = directives; NULL != d; d = d->next) {
	if (!all && d->core) {
	    continue;
	}
	text = agoo_text_append(text, "\n", 1);
	text = gql_directive_sdl(text, d, with_desc);
	text = agoo_text_append(text, "\n", 1);
    }
    return text;
}

void
gql_dump_hook(agooReq req) {
    char	buf[256];
    int		cnt;
    agooText	text = agoo_text_allocate(4094);
    bool	all = false;
    bool	with_desc = true;
    int		vlen;
    const char	*s = agoo_req_query_value(req, "all", 3, &vlen);

    if (NULL != s && 4 == vlen && 0 == strncasecmp("true", s, 4)) {
	all = true;
    }
    s = agoo_req_query_value(req, "with_desc", 9, &vlen);

    if (NULL != s && 5 == vlen && 0 == strncasecmp("false", s, 5)) {
	with_desc = false;
    }
    text = gql_schema_sdl(text, with_desc, all);
    cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 200 Okay\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n", text->len);
    text = agoo_text_prepend(text, buf, cnt);
    agoo_res_set_message(req->res, text);
}

void
gql_eval_hook(agooReq req) {
    // TBD detect introspection
    //  start resolving by callout to some global handler as needed
    //   pass target, field, args
    //   return json or gqlValue
    // for handler, if introspection then handler here else global
}

gqlDirUse
gql_dir_use_create(agooErr err, const char *name) {
    gqlDirUse	use;

    if (NULL == (use = (gqlDirUse)AGOO_MALLOC(sizeof(struct _gqlDirUse)))) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a directive useage.");
    } else {
	if (NULL == (use->dir = assure_directive(err, name))) {
	    return NULL;
	}
	use->next = NULL;
	use->args = NULL;
    }
    return use;
}

int
gql_dir_use_arg(agooErr err, gqlDirUse use, const char *key, gqlValue value) {
    gqlLink	link = gql_link_create(err, key, value);

    if (NULL == link) {
	return err->code;
    }
    link->next = use->args;
    use->args = link;
    
    return AGOO_ERR_OK;
}

void
gql_type_directive_use(gqlType type, gqlDirUse use) {
    if (NULL == type->dir) {
	type->dir = use;
    } else {
	gqlDirUse	u = type->dir;

	for (; NULL != u->next; u = u->next) {
	}
	u->next = use;
    }
}

gqlFrag
gql_fragment_create(agooErr err, const char *name, gqlType on) {
    gqlFrag	frag = (gqlFrag)AGOO_MALLOC(sizeof(struct _gqlFrag));
    
    if (NULL != frag) {
	frag->next = NULL;
	if (NULL == (frag->name = strdup(name))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of fragment name failed. %s:%d", __FILE__, __LINE__);
	    return NULL;
	}
	AGOO_ALLOC(frag->name, strlen(frag->name));
	frag->dir = NULL;
	frag->on = on;
	frag->sels = NULL;
    }
    return frag;
}

int
gql_validate(agooErr err) {

    // TBD
    // list members all the same or attempt to coerce
    // check all validation rules in doc that are not covered in the parsing
    // check the directive us in specified locations only
    // set types if they are not already set or error out

    return AGOO_ERR_OK;
}
