// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "graphql.h"
#include "gqlintro.h"
#include "gqlvalue.h"
#include "log.h"
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

gqlDir	gql_directives = NULL;

//static gqlType	schema_type = NULL;
static bool	inited = false;

static void	gql_frag_destroy(gqlFrag frag);

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
    case GQL_SCHEMA:	return "schema";
    case GQL_OBJECT:	return "object";
    case GQL_FRAG:	return "fragment";
    case GQL_INPUT:	return "input";
    case GQL_UNION:	return "union";
    case GQL_INTERFACE:	return "interface";
    case GQL_ENUM:	return "enum";
    case GQL_SCALAR:	return "scalar";
    case GQL_LIST:	return "list";
    case GQL_NON_NULL:	return "non-null";
    default:		break;
    }
    return "unknown";
}

static char*
alloc_string(agooErr err, const char *s, size_t len) {
    char	*a = NULL;

    if (NULL != s) {
	if (0 >= len) {
	    if (NULL == (a = AGOO_STRDUP(s))) {
		AGOO_ERR_MEM(err, "strdup()");
	    }
	    len = strlen(a);
	} else {
	    if (NULL == (a = AGOO_STRNDUP(s, len))) {
		AGOO_ERR_MEM(err, "strndup()");
	    }
	}
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
    type->desc = NULL;
    free_dir_uses(type->dir);
    type->dir = NULL;

    switch (type->kind) {
    case GQL_SCHEMA:
    case GQL_OBJECT:
    case GQL_INTERFACE: {
	gqlField	f;
	gqlTypeLink	link;

	while (NULL != (f = type->fields)) {
	    type->fields = f->next;
	    field_destroy(f);
	}
	while (NULL != (link = type->interfaces)) {
	    type->interfaces = link->next;
	    AGOO_FREE(link);
	}
	break;
    }
    case GQL_INPUT: {
	gqlArg	a;

	while (NULL != (a = type->args)) {
	    type->args = a->next;
	    arg_destroy(a);
	}
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
	&gql_token_type == type ||
	&gql_var_type == type ||
	&gql_id_type == type ||
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

void
gql_type_iterate(void (*fun)(gqlType type, void *ctx), void *ctx) {
    Slot	*sp = buckets;
    Slot	s;
    int		i;

    for (i = BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = s->next) {
	    fun(s->type, ctx);
	}
    }
}

gqlDir
gql_directive_get(const char *name) {
    gqlDir	dir = gql_directives;

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
	    return AGOO_ERR_MEM(err, "GraphQL Type");
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

static gqlType
type_create(agooErr err, gqlKind kind, const char *name, const char *desc, size_t dlen) {
    gqlType	type = gql_type_get(name);

    if (NULL == type) {
	if (NULL == (type = (gqlType)AGOO_MALLOC(sizeof(struct _gqlType)))) {
	    AGOO_ERR_MEM(err, "GraphQL Type");
	    return type; // type will always be NULL but C++ checker flags this as an error otherwise
	}
	if (NULL == (type->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strndup()");
	    AGOO_FREE(type);
	    return NULL;
	}
	if (NULL == (type->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE((char*)type->name);
	    AGOO_FREE(type);
	    return NULL;
	}
	type->dir = NULL;
	type->kind = kind;
	type->scalar_kind = GQL_SCALAR_UNDEF;
	type->core = false;

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
	agoo_err_set(err, AGOO_ERR_LOCK, "%s can not be modified.", name);
	return NULL;
    } else if (kind == type->kind) { // looks like it is being modified so remove all the old stuff
	agoo_err_set(err, AGOO_ERR_LOCK, "%s already exists.", name);
	return NULL;
    } else {
	agoo_err_set(err, AGOO_ERR_LOCK, "%d already exists as a %s.", name, kind_string(type->kind));
	return NULL;
    }
    return type;
}

gqlType
gql_schema_create(agooErr err, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_SCHEMA, "schema", desc, dlen);

    if (NULL != type) {
	type->fields = NULL;
	type->interfaces = NULL;
    }
    return type;
}

int
gql_init(agooErr err) {
    if (inited) {
	return AGOO_ERR_OK;
    }
    memset(buckets, 0, sizeof(buckets));
    if (AGOO_ERR_OK != gql_value_init(err) ||
	AGOO_ERR_OK != gql_intro_init(err)) {

	return err->code;
    }
    inited = true;

    return AGOO_ERR_OK;
}

extern gqlType	_gql_root_type;
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
    while (NULL != (dir = gql_directives)) {
	gql_directives = dir->next;
	dir_destroy(dir);
    }
    _gql_root_type = NULL;
    inited = false;
}

static gqlType
type_undef_create(agooErr err, const char *name) {
    return type_create(err, GQL_UNDEF, name, NULL, 0);
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
	    AGOO_ERR_MEM(err, "GraphQL Directive");
	} else {
	    dir->next = gql_directives;
	    gql_directives = dir;
	    if (NULL == (dir->name = AGOO_STRDUP(name))) {
		AGOO_ERR_MEM(err, "GraphQL Directive");
		AGOO_FREE(dir);
		return NULL;
	    }
	    dir->args = NULL;
	    dir->locs = NULL;
	    dir->defined = false;
	    dir->desc = NULL;
	}
    }
    return dir;
}

gqlType
gql_type_create(agooErr err, const char *name, const char *desc, size_t dlen, gqlTypeLink interfaces) {
    gqlType	type = type_create(err, GQL_OBJECT, name, desc, dlen);

    if (NULL != type) {
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
	       bool		required) {
    gqlField	f = (gqlField)AGOO_MALLOC(sizeof(struct _gqlField));

    if (NULL == f) {
	AGOO_ERR_MEM(err, "GraphQL Field");
    } else {
	f->next = NULL;
	if (NULL == (f->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(f);
	    return NULL;
	}
	f->type = return_type;
	if (NULL == (f->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE(f);
	    return NULL;
	}
	f->args = NULL;
	f->dir = NULL;
	f->default_value = default_value;
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
	AGOO_ERR_MEM(err, "GraphQL Field Argument");
    } else {
	a->next = NULL;
	if (NULL == (a->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(a);
	    return NULL;
	}
	a->type = type;
	if (NULL == (a->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE(a);
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

gqlArg
gql_input_arg(agooErr		err,
	      gqlType		input,
	      const char	*name,
	      gqlType		type,
	      const char	*desc,
	      size_t		dlen,
	      struct _gqlValue	*def_value,
	      bool		required) {
    gqlArg	a = (gqlArg)AGOO_MALLOC(sizeof(struct _gqlArg));

    if (NULL == a) {
	AGOO_ERR_MEM(err, "GraphQL Input Argument");
    } else {
	a->next = NULL;
	if (NULL == (a->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(a);
	    return NULL;
	}
	a->type = type;
	if (NULL == (a->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE(a);
	    return NULL;
	}
	a->default_value = def_value;
	a->dir = NULL;
	a->required = required;
	if (NULL == input->args) {
	    input->args = a;
	} else {
	    gqlArg	end;

	    for (end = input->args; NULL != end->next; end = end->next) {
	    }
	    end->next = a;
	}
    }
    return a;
}

gqlType
gql_union_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_UNION, name, desc, dlen);

    if (NULL != type) {
	type->types = NULL;
    }
    return type;
}

int
gql_union_add(agooErr err, gqlType type, gqlType member) {
    gqlTypeLink	link = (gqlTypeLink)AGOO_MALLOC(sizeof(struct _gqlTypeLink));

    if (NULL == link) {
	return AGOO_ERR_MEM(err, "GraphQL Union Value");
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

gqlType
gql_enum_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_ENUM, name, desc, dlen);

    if (NULL != type) {
	type->choices = NULL;
    }
    return type;
}

gqlEnumVal
gql_enum_append(agooErr err, gqlType type, const char *value, size_t len, const char *desc, size_t dlen) {
    gqlEnumVal	last = type->choices;
    gqlEnumVal	ev;

    for (; NULL != last; last = last->next) {
	if (0 == strcmp(value, last->value)) {
	    agoo_err_set(err, AGOO_ERR_TOO_MANY, "Enum %s already has a choice of %s.", type->name, value);
	    return NULL;
	}
    }
    if (NULL == (ev = (gqlEnumVal)AGOO_MALLOC(sizeof(struct _gqlEnumVal)))) {
	AGOO_ERR_MEM(err, "GraphQL Enum Value");
	return NULL;
    }
    if (0 >= len) {
	len = strlen(value);
    }
    if (NULL == (ev->value = AGOO_STRNDUP(value, len))) {
	AGOO_ERR_MEM(err, "strdup()");
	AGOO_FREE(ev);
	return NULL;
    }
    ev->dir = NULL;
    if (NULL == (ev->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	AGOO_FREE(ev);
	return NULL;
    }
    if (NULL == type->choices) {
	ev->next = type->choices;
	type->choices = ev;
    } else {
	for (last = type->choices; NULL != last->next; last = last->next) {
	}
	ev->next = NULL;
	last->next = ev;
    }
    return ev;
}

gqlType
gql_input_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_INPUT, name, desc, dlen);

    if (NULL != type) {
	type->args = NULL;
    }
    return type;
}

gqlType
gql_interface_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_INTERFACE, name, desc, dlen);

    if (NULL != type) {
	type->fields = NULL;
	type->interfaces = NULL;
    }
    return type;
}

// Create a scalar type that will be represented as a string.
gqlType
gql_scalar_create(agooErr err, const char *name, const char *desc, size_t dlen) {
    gqlType	type = type_create(err, GQL_SCALAR, name, desc, dlen);

    if (NULL != type) {
	type->to_json = NULL;
	type->to_sdl = NULL;
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
	    AGOO_ERR_MEM(err, "GraphQL Directive");
	    return NULL;
	}
	if (NULL == (dir->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(dir);
	    return NULL;
	}
	dir->args = NULL;
	dir->locs = NULL;
	dir->defined = true;
	dir->core = false;
	if (NULL == (dir->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE(dir);
	    return NULL;
	}
	dir->next = gql_directives;
	gql_directives = dir;
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
	AGOO_ERR_MEM(err, "GraphQL Directive Argument");
    } else {
	a->next = NULL;
	if (NULL == (a->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(a);
	    return NULL;
	}
	a->type = type;
	if (NULL == (a->desc = alloc_string(err, desc, dlen)) && AGOO_ERR_OK != err->code) {
	    AGOO_FREE(a);
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
	AGOO_ERR_MEM(err, "GraphQL Directive Location");
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
    if (NULL == (link->str = AGOO_STRNDUP(on, len))) {
	return AGOO_ERR_MEM(err, "strdup()");
    }
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
	    text = agoo_text_append_char(text, ')');
	}
    }
    return text;
}

static agooText
arg_sdl(agooText text, gqlArg a, bool with_desc, bool same_line, bool last) {
    if (with_desc) {
	text = desc_sdl(text, a->desc, 4);
    }
    if (!same_line) {
	text = agoo_text_append(text, "  ", 2);
    }
    text = agoo_text_append(text, a->name, -1);
    text = agoo_text_append(text, ": ", 2);
    if (NULL != a->type) { // should always be true
	text = agoo_text_append(text, a->type->name, -1);
    }
    if (a->required) {
	text = agoo_text_append(text, "!", 1);
    }
    if (NULL != a->default_value) {
	text = agoo_text_append(text, " = ", 3);
	text = gql_value_sdl(text, a->default_value, 0, 0);
    }
    if (same_line) {
	if (!last) {
	    text = agoo_text_append(text, ", ", 2);
	}
    } else {
	text = append_dir_use(text, a->dir);
	text = agoo_text_append_char(text, '\n');
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
	    text = arg_sdl(text, a, with_desc, true, NULL == a->next);
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
    text = agoo_text_append_char(text, '\n');

    return text;
}

agooText
gql_type_sdl(agooText text, gqlType type, bool with_desc) {
    if (with_desc) {
	desc_sdl(text, type->desc, 0);
    }
    switch (type->kind) {
    case GQL_SCHEMA: {
	gqlField	f;

	text = agoo_text_append(text, "schema", 6);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);
	for (f = type->fields; NULL != f; f = f->next) {
	    text = field_sdl(text, f, with_desc);
	}
	text = agoo_text_append(text, "}\n", 2);
	break;
    }
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
	gqlArg	a;

	text = agoo_text_append(text, "input ", 6);
	text = agoo_text_append(text, type->name, -1);
	text = append_dir_use(text, type->dir);
	text = agoo_text_append(text, " {\n", 3);
	for (a = type->args; NULL != a; a = a->next) {
	    text = arg_sdl(text, a, with_desc, false, NULL == a->next);
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
	    text = arg_sdl(text, a, with_desc, true, NULL == a->next);
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
    for (d = gql_directives; NULL != d; d = d->next) {
	if (!all && d->core) {
	    continue;
	}
	text = agoo_text_append(text, "\n", 1);
	text = gql_directive_sdl(text, d, with_desc);
	text = agoo_text_append(text, "\n", 1);
    }
    return text;
}

gqlDirUse
gql_dir_use_create(agooErr err, const char *name) {
    gqlDirUse	use;

    if (NULL == (use = (gqlDirUse)AGOO_MALLOC(sizeof(struct _gqlDirUse)))) {
	AGOO_ERR_MEM(err, "GraphQL Directive Usage");
    } else {
	if (NULL == (use->dir = assure_directive(err, name))) {
	    AGOO_FREE(use);
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

int
gql_type_directive_use(agooErr err, gqlType type, gqlDirUse use) {
    if (NULL == type->dir) {
	type->dir = use;
    } else if (0 == strcmp(use->dir->name, type->dir->dir->name)) {
	return agoo_err_set(err, AGOO_ERR_TOO_MANY, "Directive %s on type %s is a duplicate.", use->dir->name, type->name);
    } else {
	gqlDirUse	u = type->dir;

	for (; NULL != u->next; u = u->next) {
	    if (0 == strcmp(use->dir->name, u->dir->name)) {
		return agoo_err_set(err, AGOO_ERR_TOO_MANY, "Directive %s on type %s is a duplicate.", use->dir->name, type->name);
	    }
	}
	u->next = use;
    }
    return AGOO_ERR_OK;
}

bool
gql_type_has_directive_use(gqlType type, const char *dir) {
    gqlDirUse	u = type->dir;

    for (; NULL != u; u = u->next) {
	if (0 == strcmp(dir, u->dir->name)) {
	    return true;
	}
    }
    return false;
}

gqlFrag
gql_fragment_create(agooErr err, const char *name, gqlType on) {
    gqlFrag	frag = (gqlFrag)AGOO_MALLOC(sizeof(struct _gqlFrag));

    if (NULL == frag) {
	AGOO_ERR_MEM(err, "GraphQL Fragment");
    } else {
	frag->next = NULL;
	if (NULL != name) {
	    if (NULL == (frag->name = AGOO_STRDUP(name))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(frag);
		return NULL;
	    }
	} else {
	    frag->name = NULL;
	}
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

gqlDoc
gql_doc_create(agooErr err) {
    gqlDoc	doc = (gqlDoc)AGOO_MALLOC(sizeof(struct _gqlDoc));

    if (NULL == doc) {
	AGOO_ERR_MEM(err, "GraphQL Document");
    } else {
	doc->ops = NULL;
	doc->vars = NULL;
	doc->frags = NULL;
    }
    return doc;
}

gqlOp
gql_op_create(agooErr err, const char *name, gqlOpKind kind) {
    gqlOp	op = (gqlOp)AGOO_MALLOC(sizeof(struct _gqlOp));

    if (NULL == op) {
	AGOO_ERR_MEM(err, "GraphQL Operation");
    } else {
	op->next = NULL;
	if (NULL != name && '\0' != *name) {
	    if (NULL == (op->name = AGOO_STRDUP(name))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(op);
		return NULL;
	    }
	} else {
	    op->name = NULL;
	}
	op->vars = NULL;
	op->dir = NULL;
	op->sels = NULL;
	op->kind = kind;
    }
    return op;
}

static void
var_destroy(gqlVar var) {
    AGOO_FREE((char*)var->name);
    if (NULL != var->value) {
	gql_value_destroy(var->value);
    }
    AGOO_FREE(var);
}

static void
sel_arg_destroy(gqlSelArg arg) {
    AGOO_FREE((char*)arg->name);
    if (NULL != arg->value) {
	gql_value_destroy(arg->value);
    }
    AGOO_FREE(arg);
}

static void
sel_destroy(gqlSel sel) {
    gqlSelArg	arg;
    gqlSel	s;

    AGOO_FREE((char*)sel->alias);
    AGOO_FREE((char*)sel->name);
    while (NULL != (arg = sel->args)) {
	sel->args = arg->next;
	sel_arg_destroy(arg);
    }
    free_dir_uses(sel->dir);
    while (NULL != (s = sel->sels)) {
	sel->sels = s->next;
	sel_destroy(s);
    }
    AGOO_FREE((char*)sel->frag);
    if (NULL != sel->inline_frag) {
	gql_frag_destroy(sel->inline_frag);
    }
    AGOO_FREE(sel);
}

void
gql_op_destroy(gqlOp op) {
    gqlSel	sel;
    gqlVar	var;

    AGOO_FREE((char*)op->name);
    while (NULL != (var = op->vars)) {
	op->vars = var->next;
	var_destroy(var);
    }
    free_dir_uses(op->dir);
    while (NULL != (sel = op->sels)) {
	op->sels = sel->next;
	sel_destroy(sel);
    }
    AGOO_FREE(op);
}

void
gql_frag_destroy(gqlFrag frag) {
    gqlSel	sel;

    AGOO_FREE((char*)frag->name);
    free_dir_uses(frag->dir);
    while (NULL != (sel = frag->sels)) {
	frag->sels = sel->next;
	sel_destroy(sel);
    }
    AGOO_FREE(frag);
}

void
gql_doc_destroy(gqlDoc doc) {
    gqlOp	op;
    gqlFrag	frag;
    gqlVar	var;

    while (NULL != (op = doc->ops)) {
	doc->ops = op->next;
	gql_op_destroy(op);
    }
    while (NULL != (var = doc->vars)) {
	doc->vars = var->next;
	var_destroy(var);
    }
    while (NULL != (frag = doc->frags)) {
	doc->frags = frag->next;
	gql_frag_destroy(frag);
    }
    AGOO_FREE(doc);
}

static agooText
sel_sdl(agooText text, gqlSel sel, int depth) {
    int	indent = depth * 2;

    if ((int)sizeof(spaces) <= indent) {
	indent = sizeof(spaces) - 1;
    }
    text = agoo_text_append(text, spaces, indent);
    if (NULL != sel->frag) {
	text = agoo_text_append(text, "...", 3);
	text = agoo_text_append(text, sel->frag, -1);
	text = append_dir_use(text, sel->dir);
    } else if (NULL != sel->inline_frag) {
	if (NULL == sel->inline_frag->on) {
	    text = agoo_text_append(text, "...", 3);
	} else {
	    text = agoo_text_append(text, "... on ", 7);
	    text = agoo_text_append(text, sel->inline_frag->on->name, -1);
	}
	text = append_dir_use(text, sel->inline_frag->dir);
	if (NULL != sel->inline_frag->sels) {
	    gqlSel	s;
	    int		d2 = depth + 1;

	    text = agoo_text_append(text, " {\n", 3);
	    for (s = sel->inline_frag->sels; NULL != s; s = s->next) {
		text = sel_sdl(text, s, d2);
	    }
	    text = agoo_text_append(text, spaces, indent);
	    text = agoo_text_append(text, "}", 1);
	}
    } else {
	if (NULL != sel->alias) {
	    text = agoo_text_append(text, sel->alias, -1);
	    text = agoo_text_append(text, ": ", 2);
	}
	text = agoo_text_append(text, sel->name, -1);
	if (NULL != sel->args) {
	    gqlSelArg	arg;

	    text = agoo_text_append(text, "(", 1);
	    for (arg = sel->args; NULL != arg; arg = arg->next) {
		text = agoo_text_append(text, arg->name, -1);
		text = agoo_text_append(text, ": ", 2);
		if (NULL != arg->var) {
		    text = agoo_text_append(text, "$", 1);
		    text = agoo_text_append(text, arg->var->name, -1);
		} else if (NULL != arg->value) {
		    text = gql_value_sdl(text, arg->value, 0, 0);
		} else {
		    text = agoo_text_append(text, "null", 4);
		}
		if (NULL != arg->next) {
		    text = agoo_text_append(text, ", ", 2);
		}
	    }
	    text = agoo_text_append(text, ")", 1);
	}
	text = append_dir_use(text, sel->dir);
	if (NULL != sel->sels) {
	    gqlSel	s;
	    int		d2 = depth + 1;

	    text = agoo_text_append(text, " {\n", 3);
	    for (s = sel->sels; NULL != s; s = s->next) {
		text = sel_sdl(text, s, d2);
	    }
	    text = agoo_text_append(text, spaces, indent);
	    text = agoo_text_append(text, "}", 1);
	}
    }
    text = agoo_text_append(text, "\n", 1);

    return text;
}

static agooText
op_sdl(agooText text, gqlOp op) {
    gqlSel	sel;

    switch (op->kind) {
    case GQL_MUTATION:
	text = agoo_text_append(text, "mutation", 8);
	break;
    case GQL_SUBSCRIPTION:
	text = agoo_text_append(text, "subscription", 12);
	break;
    case GQL_QUERY:
    default:
	text = agoo_text_append(text, "query", 5);
	break;
    }
    if (NULL != op->name) {
	text = agoo_text_append(text, " ", 1);
	text = agoo_text_append(text, op->name, -1);
    }
    if (NULL != op->vars) {
	gqlVar	var;

	text = agoo_text_append(text, "(", 1);
	for (var = op->vars; NULL != var; var = var->next) {
	    text = agoo_text_append(text, "$", 1);
	    text = agoo_text_append(text, var->name, -1);
	    text = agoo_text_append(text, ": ", 2);

	    text = agoo_text_append(text, var->type->name, -1);
	    if (NULL != var->value) {
		text = agoo_text_append(text, " = ", 3);
		text = gql_value_sdl(text, var->value, 0, 0);
	    }
	    if (NULL != var->next) {
		text = agoo_text_append(text, ", ", 2);
	    }
	}
	text = agoo_text_append(text, ")", 1);
    }
    text = append_dir_use(text, op->dir);

    text = agoo_text_append(text, " {\n", 3);
    for (sel = op->sels; NULL != sel; sel = sel->next) {
	text = sel_sdl(text, sel, 1);
    }
    text = agoo_text_append(text, "}\n", 2);

    return text;
}

static agooText
frag_sdl(agooText text, gqlFrag frag) {
    gqlSel	sel;

    text = agoo_text_append(text, "fragment ", 9);
    text = agoo_text_append(text, frag->name, -1);
    text = agoo_text_append(text, " on ", 4);
    text = agoo_text_append(text, frag->on->name, -1);
    text = append_dir_use(text, frag->dir);
    text = agoo_text_append(text, " {\n", 3);
    for (sel = frag->sels; NULL != sel; sel = sel->next) {
	text = sel_sdl(text, sel, 1);
    }
    text = agoo_text_append(text, "}\n", 2);

    return text;
}

agooText
gql_doc_sdl(gqlDoc doc, agooText text) {
    gqlOp	op;
    gqlFrag	frag;

    for (op = doc->ops; NULL != op; op = op->next) {
	op_sdl(text, op);
    }
    for (frag = doc->frags; NULL != frag; frag = frag->next) {
	text = frag_sdl(text, frag);
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
    if (NULL == (text = agoo_text_prepend(text, buf, cnt))) {
	agoo_log_cat(&agoo_error_cat, "Failed to allocate memory for a GraphQL dump.");
    }
    agoo_res_message_push(req->res, text);
}

gqlField
gql_type_get_field(gqlType type, const char *field) {
    gqlField	f;

    if (NULL == type) {
	return NULL;
    }
    switch (type->kind) {
    case GQL_SCHEMA:
    case GQL_OBJECT:
    case GQL_INTERFACE:
	for (f = type->fields; NULL != f; f = f->next) {
	    if (0 == strcmp(field, f->name)) {
		return f;
	    }
	}
	break;
    case GQL_UNION:
	// TBD
	break;
    case GQL_LIST:
	return gql_type_get_field(type->base, field);
	break;
    default:
	// TBD
	break;
    }
    return NULL;
}
