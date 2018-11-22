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

static gqlType	query_type = NULL;
static gqlType	mutation_type = NULL;
static gqlType	subscription_type = NULL;
static gqlType	schema_type = NULL;
static gqlDir	directives = NULL;

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
		    free((char*)a->type_name);
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
	case GQL_UNION: {
	    gqlTypeLink	link;

	    while (NULL != (link = type->types)) {
		type->types = link->next;
		free(link->name);
		free(link);
	    }
	    break;
	}
	case GQL_ENUM: {
	    gqlStrLink	link;

	    while (NULL != (link = type->choices)) {
		type->choices = link->next;
		free(link->str);
		free(link);
	    }
	    break;
	}
	default:
	    break;
	}
	DEBUG_FREE(mem_graphql_type, type);
	free(type);
    }
}

static void
dir_destroy(gqlDir dir) {
    gqlArg	a;
    gqlStrLink	link;
    
    free((char*)dir->name);
    free((char*)dir->desc);
    while (NULL != (a = dir->args)) {
	dir->args = a->next;
	free((char*)a->name);
	free((char*)a->type_name);
	free((char*)a->desc);
	gql_value_destroy(a->default_value);
	DEBUG_FREE(mem_graphql_arg, a);
	free(a);
    }
    while (NULL != (link = dir->locs)) {
	dir->locs = link->next;
	free(link->str);
	free(link);
    }
    DEBUG_FREE(mem_graphql_directive, dir);
    free(dir);
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
	if (NULL == (s = (Slot)malloc(sizeof(struct _slot)))) {
	    return agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL type.");
	}
	DEBUG_ALLOC(mem_graphql_slot, s);
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
		DEBUG_FREE(mem_graphql_slot, s);
		free(s);

		break;
	    }
	    prev = s;
	}
    }
}

int
gql_init(agooErr err) {
    memset(buckets, 0, sizeof(buckets));

    if (AGOO_ERR_OK != gql_value_init(err) ||
	AGOO_ERR_OK != gql_intro_init(err)) {

	return err->code;
    }
    if (
	NULL == (query_type = gql_type_create(err, "Query", "The GraphQL root Query.", -1, false, NULL)) ||
	NULL == (mutation_type = gql_type_create(err, "Mutation", "The GraphQL root Mutation.", -1, false, NULL)) ||
	NULL == (subscription_type = gql_type_create(err, "Subscription", "The GraphQL root Subscription.", -1, false, NULL)) ||
	NULL == (schema_type = gql_type_create(err, "schema", "The GraphQL root Object.", -1, false, NULL)) ||
	NULL == gql_type_field(err, schema_type, "query", query_type, "Root level query.", false, false, false, NULL) ||
	NULL == gql_type_field(err, schema_type, "mutation", mutation_type, "Root level mutation.", false, false, false, NULL) ||
	NULL == gql_type_field(err, schema_type, "subscription", subscription_type, "Root level subscription.", false, false, false, NULL)) {

	return err->code;
    }
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
    while (NULL != (dir = directives)) {
	directives = dir->next;
	dir_destroy(dir);
    }
}

static gqlType
type_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = (gqlType)malloc(sizeof(struct _gqlType));

    if (NULL == type) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Type.");
    } else {
	DEBUG_ALLOC(mem_graphql_type, type);
	type->name = strdup(name);
	if (NULL == desc) {
	    type->desc = NULL;
	} else {
	    if (0 >= dlen) {
		type->desc = strdup(desc);
	    } else {
		type->desc = strndup(desc, dlen);
	    }
	}
	type->locked = locked;
	type->core = false;
	type->dir = NULL;

	if (AGOO_ERR_OK != gql_type_set(err, type)) {
	    gql_type_destroy(type);
	    type = NULL;
	}
    }
    return type;
}

agooText
gql_object_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

agooText
gql_object_to_graphql(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_type_create(agooErr err, const char *name, const char *desc, int dlen, bool locked, gqlType *interfaces) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_OBJECT;
	type->to_json = gql_object_to_json;
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
		    agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL type interfaces.");
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
gql_type_field(agooErr err,
	       gqlType type,
	       const char *name,
	       gqlType return_type,
	       const char *desc,
	       bool required,
	       bool list,
	       bool not_empty,
	       gqlResolveFunc resolve) {
    gqlField	f = (gqlField)malloc(sizeof(struct _gqlField));
    
    if (NULL == f) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL field.");
    } else {
	DEBUG_ALLOC(mem_graphql_field, f);
	f->next = NULL;
	f->name = strdup(name);
	f->type = return_type;
	if (NULL == desc) {
	    f->desc = NULL;
	} else {
	    f->desc = strdup(desc);
	}
	f->reason = NULL;
	f->args = NULL;
	f->dir = NULL;
	f->resolve = resolve;
	f->required = required;
	f->list = list;
	f->not_empty = not_empty;
	f->deprecated = false;
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
	      struct _gqlValue	*def_value,
	      bool		required) {
    gqlArg	a = (gqlArg)malloc(sizeof(struct _gqlArg));
    
    if (NULL == a) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL field argument.");
    } else {
	DEBUG_ALLOC(mem_graphql_arg, a);
	a->next = NULL;
	a->name = strdup(name);
	a->type = type;
	if (NULL == desc) {
	    a->desc = NULL;
	} else {
	    a->desc = strdup(desc);
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

gqlType
gql_union_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_UNION;
	type->to_json = gql_union_to_json;
	type->types = NULL;
    }
    return type;
}

int
gql_union_add(agooErr err, gqlType type, const char *name, int len) {
    gqlTypeLink	link = (gqlTypeLink)malloc(sizeof(gqlTypeLink));

    if (NULL == link) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Union value.");
    }
    if (0 >= len) {
	len = (int)strlen(name);
    }
    link->name = strndup(name, len);
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

gqlType
gql_enum_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_ENUM;
	type->to_json = gql_enum_to_json;
	type->choices = NULL;
    }
    return type;
}

int
gql_enum_add(agooErr err, gqlType type, const char *value, int len) {
    gqlStrLink	link = (gqlStrLink)malloc(sizeof(gqlStrLink));

    if (NULL == link) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Enum value.");
    }
    if (0 >= len) {
	len = (int)strlen(value);
    }
    link->next = type->choices;
    type->choices = link;
    link->str = strndup(value, len);
    
    return AGOO_ERR_OK;
}

int
gql_enum_append(agooErr err, gqlType type, const char *value, int len) {
    gqlStrLink	link = (gqlStrLink)malloc(sizeof(gqlStrLink));

    if (NULL == link) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL Enum value.");
    }
    if (0 >= len) {
	len = (int)strlen(value);
    }
    link->str = strndup(value, len);
    if (NULL == type->choices) {
	link->next = type->choices;
	type->choices = link;
    } else {
	gqlStrLink	last = type->choices;

	for (; NULL != last->next; last = last->next) {
	}
	link->next = NULL;
	last->next = link;
    }
    return AGOO_ERR_OK;
}

static agooText
fragment_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_fragment_create(agooErr err, const char *name, const char *desc, int dlen, bool locked, gqlType on) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_FRAG;
	type->to_json = fragment_to_json;
	type->fields = NULL;
	type->on = on;
    }
    return type;
}

static agooText
input_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_input_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_INPUT;
	type->to_json = input_to_json;
	type->fields = NULL;
    }
    return type;
}

static agooText
interface_to_json(agooText text, gqlValue value, int indent, int depth) {
    // TBD
    return text;
}

gqlType
gql_interface_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_INTERFACE;
	type->to_json = interface_to_json;
	type->fields = NULL;
    }
    return type;
}

static agooText
scalar_to_json(agooText text, gqlValue value, int indent, int depth) {
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
gql_scalar_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlType	type = type_create(err, name, desc, dlen, locked);

    if (NULL != type) {
	type->kind = GQL_SCALAR;
	type->to_json = scalar_to_json;
	type->coerce = NULL;
	type->destroy = NULL;
    }
    return type;
}

gqlDir
gql_directive_create(agooErr err, const char *name, const char *desc, int dlen, bool locked) {
    gqlDir	dir;
    
    if (NULL == (dir = (gqlDir)malloc(sizeof(struct _gqlDir)))) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive.");
	return NULL;
    }
    DEBUG_ALLOC(mem_graphql_directive, dir);
    dir->next = directives;
    directives = dir;
    dir->name = strdup(name);
    dir->args = NULL;
    dir->locs = NULL;
    dir->locked = locked;
    if (NULL == desc) {
	dir->desc = NULL;
    } else {
	if (0 >= dlen) {
	    dir->desc = strdup(desc);
	} else {
	    dir->desc = strndup(desc, dlen);
	}
    }
    return dir;
}

gqlArg
gql_dir_arg(agooErr 		err,
	    gqlDir 		dir,
	    const char 		*name,
	    const char 		*type_name,
	    const char	 	*desc,
	    int			dlen,
	    struct _gqlValue	*def_value,
	    bool 		required) {

    gqlArg	a = (gqlArg)malloc(sizeof(struct _gqlArg));
    
    if (NULL == a) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL directive argument.");
    } else {
	DEBUG_ALLOC(mem_graphql_arg, a);
	a->next = NULL;
	a->name = strdup(name);
	a->type_name = strdup(type_name);
	a->type = NULL;
	if (NULL == desc) {
	    a->desc = NULL;
	} else if (0 < dlen) {
	    a->desc = strdup(desc);
	} else {
	    a->desc = strndup(desc, dlen);
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
    gqlStrLink	link = (gqlStrLink)malloc(sizeof(gqlStrLink));
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
    link->str = strndup(on, len);
    
    return AGOO_ERR_OK;
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
    if (a->required) {
	text = agoo_text_append(text, "!: ", 3);
    } else {
	text = agoo_text_append(text, ": ", 2);
    }
    text = agoo_text_append(text, a->type_name, -1);
    if (NULL != a->default_value) {
	text = agoo_text_append(text, " = ", 3);
	text = gql_value_json(text, a->default_value, 0, 0);
    }
    if (!last) {
	text = agoo_text_append(text, ", ", 2);
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
    if (f->list) {
	text = agoo_text_append(text, "[", 1);
	text = agoo_text_append(text, f->type->name, -1);
	if (f->not_empty) {
	    text = agoo_text_append(text, "!", 1);
	}
	text = agoo_text_append(text, "]", 1);
    } else {
	text = agoo_text_append(text, f->type->name, -1);
    }
    if (f->required) {
	text = agoo_text_append(text, "!", 1);
    }
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
	    gqlType	*tp = type->interfaces;

	    text = agoo_text_append(text, " implements ", 12);
	    for (; NULL != *tp; tp++) {
		text = agoo_text_append(text, (*tp)->name, -1);
		if (NULL != *(tp + 1)) {
		    text = agoo_text_append(text, ", ", 2);
		}
	    }
	}
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
	text = agoo_text_append(text, " = ", 3);
	for (link = type->types; NULL != link; link = link->next) {
	    text = agoo_text_append(text, link->name, -1);
	    if (NULL != link->next) {
		text = agoo_text_append(text, " | ", 3);
	    }
	}
	break;
    }
    case GQL_ENUM: {
	gqlStrLink	link;;
	
	text = agoo_text_append(text, "enum ", 5);
	text = agoo_text_append(text, type->name, -1);
	text = agoo_text_append(text, " {\n", 3);
	for (link = type->choices; NULL != link; link = link->next) {
	    text = agoo_text_append(text, "  ", 2);
	    text = agoo_text_append(text, link->str, -1);
	    text = agoo_text_append(text, "\n", 1);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
    case GQL_SCALAR:
	text = agoo_text_append(text, "scalar ", 7);
	text = agoo_text_append(text, type->name, -1);
	text = agoo_text_append(text, "\n", 1);
	break;
    case GQL_INTERFACE: {
	gqlField	f;

	text = agoo_text_append(text, "interface ", 10);
	text = agoo_text_append(text, type->name, -1);
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
    cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 200 Okay\r\nContent-Type: application/graphql\r\nContent-Length: %ld\r\n\r\n", text->len);
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
