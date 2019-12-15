// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "doc.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

static const char	enum_str[] = "enum";
static const char	extend_str[] = "extend";
static const char	input_str[] = "input";
static const char	interface_str[] = "interface";
static const char	mutation_str[] = "mutation";
static const char	query_str[] = "query";
static const char	schema_str[] = "schema";
static const char	subscription_str[] = "subscription";
static const char	type_str[] = "type";
static const char	union_str[] = "union";

static int	make_sel(agooErr err, agooDoc doc, gqlDoc gdoc, gqlOp op, gqlSel *parentp);

static int
extract_desc(agooErr err, agooDoc doc, const char **descp, size_t *lenp) {
    agoo_doc_skip_white(doc);
    *descp = NULL;
    *lenp = 0;
    if ('"' == *doc->cur) {
	const char	*desc_end = NULL;
	const char	*desc = doc->cur + 1;

    	if (AGOO_ERR_OK != agoo_doc_read_string(err, doc)) {
	    return err->code;
	}
	if ('"' == *desc && '"' == desc[1]) { // must be a """
	    desc += 2;
	    desc_end = doc->cur - 3;
	} else {
	    desc_end = doc->cur - 1;
	}
	*descp = desc;
	*lenp = desc_end - *descp;
    }
    return AGOO_ERR_OK;
}

static size_t
read_name(agooErr err, agooDoc doc, char *name, size_t max) {
    size_t	nlen;
    const char	*start = doc->cur;

    agoo_doc_read_token(doc);
    if (doc->cur == start) {
	agoo_doc_err(doc, err, "Name not provided");
	return 0;
    }
    nlen = doc->cur - start;
    if (max <= nlen) {
	agoo_doc_err(doc, err, "Name too long");
	return 0;
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';

    return nlen;
}

static int
extract_name(agooErr err, agooDoc doc, const char *key, int klen, char *name, size_t max) {
    if (0 != strncmp(doc->cur, key, klen)) {
	return agoo_doc_err(doc, err, "Expected %s key word", key);
    }
    doc->cur += klen;
    if (0 == agoo_doc_skip_white(doc)) {
	return agoo_doc_err(doc, err, "Expected %s key word", key);
    }
    if (0 == read_name(err, doc, name, max)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
make_use_arg(agooErr err, agooDoc doc, gqlDirUse use) {
    char	name[256];
    gqlValue	value = NULL;

    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected ':'");
    }
    doc->cur++;
    if (NULL == (value = agoo_doc_read_value(err, doc, NULL))) {
	return err->code;
    }
    if (AGOO_ERR_OK == gql_dir_use_arg(err, use, name, value)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
extract_dir_use(agooErr err, agooDoc doc, gqlDirUse *uses) {
    char	name[256];
    gqlDirUse	use;

    agoo_doc_skip_white(doc);
    if ('@' != *doc->cur) {
	return AGOO_ERR_OK;
    }
    doc->cur++;
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    if (NULL == (use = gql_dir_use_create(err, name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('(' == *doc->cur) {
	doc->cur++;
	while (doc->cur < doc->end) {
	    if (AGOO_ERR_OK != make_use_arg(err, doc, use)) {
		return err->code;
	    }
	    agoo_doc_skip_white(doc);
	    if (')' == *doc->cur) {
		doc->cur++;
		break;
	    }
	}
    }
    if (NULL == *uses) {
	*uses = use;
    } else {
	gqlDirUse	u = *uses;

	for (; NULL != u->next; u = u->next) {
	}
	u->next = use;
    }
    return AGOO_ERR_OK;
}

static int
make_scalar(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    char	name[256];
    gqlType	type;

    if (AGOO_ERR_OK != extract_name(err, doc, "scalar", 6, name, sizeof(name))) {
	return err->code;
    }
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_scalar_create(err, name, desc, len))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
read_type(agooErr err, agooDoc doc, gqlType *typep, bool *required) {
    agoo_doc_skip_white(doc);
    if ('[' == *doc->cur) {
	gqlType	base;
	bool	not_empty = false;

	doc->cur++;
	if (AGOO_ERR_OK != read_type(err, doc, &base, &not_empty)) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
	if (']' != *doc->cur) {
	    return agoo_doc_err(doc, err, "List type not terminated with a ]");
	}
	doc->cur++;
	if (NULL == (*typep = gql_assure_list(err, base, not_empty))) {
	    return err->code;
	}
    } else {
	char	name[256];

	if (0 == read_name(err, doc, name, sizeof(name))) {
	    return err->code;
	}
	if (NULL == (*typep = gql_assure_type(err, name))) {
	    return err->code;
	}
    }
    *required = false;
    agoo_doc_skip_white(doc);
    if ('!' == *doc->cur) {
	*required = true;
	doc->cur++;
    }
    return AGOO_ERR_OK;
}

static int
make_enum(agooErr err, agooDoc doc, const char *desc, size_t dlen, bool x) {
    char	name[256];
    const char	*start;
    gqlType	type;
    gqlDirUse	uses = NULL;
    gqlEnumVal	ev;
    size_t	len;

    if (AGOO_ERR_OK != extract_name(err, doc, enum_str, sizeof(enum_str) - 1, name, sizeof(name))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('{' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected '{'");
    }
    doc->cur++;
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_enum_create(err, name, desc, dlen))) {
	return err->code;
    }
    type->dir = uses;
    while (doc->cur < doc->end) {
	desc = NULL;
	uses = NULL;

	if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	    goto ERROR;
	}
	start = doc->cur;
	agoo_doc_read_token(doc);
	len = doc->cur - start;
	if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	    goto ERROR;
	}
	if (doc->cur == start) {
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    agoo_doc_err(doc, err, "Invalid Enum value");
	    goto ERROR;
	}
	if (NULL == (ev = gql_enum_append(err, type, start, len, desc, dlen))) {
	    goto ERROR;
	}
	ev->dir = uses;
    }
    return AGOO_ERR_OK;
ERROR:
    if (!x) {
	gql_type_destroy(type);
    }
    return err->code;
}

static int
make_union(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    char	name[256];
    gqlType	type;
    gqlType	member;
    bool	required;

    if (AGOO_ERR_OK != extract_name(err, doc, union_str, sizeof(union_str) - 1, name, sizeof(name))) {
	return err->code;
    }
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_union_create(err, name, desc, len))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	goto ERROR;
    }
    agoo_doc_skip_white(doc);
    if ('=' != *doc->cur) {
	agoo_doc_err(doc, err, "Expected '='");
	goto ERROR;
    }
    doc->cur++;
    agoo_doc_skip_white(doc);

    while (doc->cur < doc->end) {
	if (AGOO_ERR_OK != read_type(err, doc, &member, &required)) {
	    goto ERROR;
	}
 	if (AGOO_ERR_OK != gql_union_add(err, type, member)) {
	    goto ERROR;
	}
	agoo_doc_skip_white(doc);
	if ('|' != *doc->cur) {
	    break;
	}
	doc->cur++; // skip |
    }
    return AGOO_ERR_OK;
ERROR:
    if (!x) {
	gql_type_destroy(type);
    }
    return err->code;
}

static int
make_dir_arg(agooErr err, agooDoc doc, gqlDir dir) {
    char	name[256];
    char	type_name[256];
    gqlType	type;
    const char	*start;
    const char	*desc = NULL;
    size_t	dlen;
    size_t	nlen;
    bool	required = false;
    gqlValue	dv = NULL;

    if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    if (doc->cur == start) {
	return agoo_doc_err(doc, err, "Argument name not provided");
    }
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected ':'");
    }
    nlen = doc->cur - start;
    if (sizeof(name) <= nlen) {
	return agoo_doc_err(doc, err, "Name too long");
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';
    doc->cur++;

    // read type
    agoo_doc_skip_white(doc);

    start = doc->cur;
    agoo_doc_read_token(doc);
    if (doc->cur == start) {
	return agoo_doc_err(doc, err, "Argument type not provided");
    }
    nlen = doc->cur - start;
    if (sizeof(type_name) <= nlen) {
	return agoo_doc_err(doc, err, "Type name too long");
    }
    strncpy(type_name, start, nlen);
    type_name[nlen] = '\0';

    if (NULL == (type = gql_assure_type(err, type_name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('!' == *doc->cur) {
	required = true;
	doc->cur++;
    } else if ('=' == *doc->cur) {
	if (NULL == (dv = agoo_doc_read_value(err, doc, type))) {
	    return err->code;
	}
    }
    agoo_doc_skip_white(doc);
    if ('@' == *doc->cur) {
	// TBD directive
    }
    if (NULL == gql_dir_arg(err, dir, name, type, desc, dlen, dv, required)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
make_directive(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlDir	dir;
    size_t	nlen;

    if (0 != strncmp(doc->cur, "directive", 9)) {
	return agoo_doc_err(doc, err, "Expected directive key word");
    }
    doc->cur += 9;
    if (0 == agoo_doc_skip_white(doc)) {
	return agoo_doc_err(doc, err, "Expected directive key word");
    }
    if ('@' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected '@'");
    }
    doc->cur++;
    if (0 == (nlen = read_name(err, doc, name, sizeof(name)))) {
	return err->code;
    }
    if (NULL == (dir = gql_directive_create(err, name, desc, len))) {
	return err->code;
    }
    if ('(' == *doc->cur) {
	doc->cur++;
	while (doc->cur < doc->end) {
	    if (AGOO_ERR_OK != make_dir_arg(err, doc, dir)) {
		return err->code;
	    }
	    agoo_doc_skip_white(doc);
	    if (')' == *doc->cur) {
		doc->cur++;
		break;
	    }
	}
    }
    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);

    if (2 != doc->cur - start || 0 != strncmp(start, "on", 2)) {
	return agoo_doc_err(doc, err, "Expected 'on'");
    }
    while (doc->cur < doc->end) {
	agoo_doc_skip_white(doc);
	start = doc->cur;
	agoo_doc_read_token(doc);
	if (AGOO_ERR_OK != gql_directive_on(err, dir, start, (int)(doc->cur - start))) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
	if ('|' != *doc->cur) {
	    break;
	}
	doc->cur++; // skip |
    }
    return AGOO_ERR_OK;
}

static int
make_field_arg(agooErr err, agooDoc doc, gqlField field) {
    char	name[256];
    gqlType	type;
    const char	*desc = NULL;
    size_t	dlen;
    bool 	required = false;
    gqlValue	dval = NULL;

    if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected :");
    }
    doc->cur++;

    if (AGOO_ERR_OK != read_type(err, doc, &type, &required)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);

    switch (*doc->cur) {
    case '=':
	if (NULL == (dval = agoo_doc_read_value(err, doc, type))) {
	    return err->code;
	}
	break;
    case '@':
	// TBD read directives
	break;
    default: // ) or next arg
	break;
    }
    if (NULL == gql_field_arg(err, field, name, type, desc, dlen, dval, required)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

static int
make_field(agooErr err, agooDoc doc, gqlType type) {
    char	name[256];
    gqlType	return_type;
    const char	*arg_start = NULL;
    gqlField	field;
    gqlDirUse	uses = NULL;
    gqlValue	dval = NULL;
    const char	*desc = NULL;
    size_t	dlen;
    bool 	required = false;

    if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    switch (*doc->cur) {
    case '(':
	doc->cur++;
	arg_start = doc->cur;
	if (!agoo_doc_skip_to(doc, ')')) {
	    return agoo_doc_err(doc, err, "Argument list not terminated with a )");
	}
	doc->cur++;
	agoo_doc_skip_white(doc);
	if (':' != *doc->cur) {
	    return agoo_doc_err(doc, err, "Expected :");
	}
	doc->cur++;
	break;
    case ':':
	doc->cur++;
	// okay
	break;
    default:
	return agoo_doc_err(doc, err, "Expected : or (");
    }
    if (AGOO_ERR_OK != read_type(err, doc, &return_type, &required)) {
	return err->code;
    }
    if ('=' == *doc->cur) {
	doc->cur++;
	if (NULL == (dval = agoo_doc_read_value(err, doc, return_type))) {
	    return err->code;
	}
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	return err->code;
    }
    if (NULL == (field = gql_type_field(err, type, name, return_type, dval, desc, dlen, required))) {
	return err->code;
    }
    field->dir = uses;
    if (NULL != arg_start) {
	const char	*cur = doc->cur;

	doc->cur = arg_start;
	while (true) {
	    if (AGOO_ERR_OK != make_field_arg(err, doc, field)) {
		return err->code;
	    }
	    agoo_doc_skip_white(doc);
	    if (')' == *doc->cur) {
		break;
	    }
	}
	doc->cur = cur;
    }
    return AGOO_ERR_OK;
}

static int
make_input_arg(agooErr err, agooDoc doc, gqlType type) {
    char	name[256];
    gqlType	return_type;
    gqlArg	arg;
    gqlDirUse	uses = NULL;
    gqlValue	dval = NULL;
    const char	*desc = NULL;
    size_t	dlen;
    bool 	required = false;

    if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected :");
    }
    doc->cur++;
    if (AGOO_ERR_OK != read_type(err, doc, &return_type, &required)) {
	return err->code;
    }
    if ('=' == *doc->cur) {
	doc->cur++;
	if (NULL == (dval = agoo_doc_read_value(err, doc, return_type))) {
	    return err->code;
	}
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	return err->code;
    }
    if (NULL == (arg = gql_input_arg(err, type, name, return_type, desc, dlen, dval, required))) {
	return err->code;
    }
    arg->dir = uses;

    return AGOO_ERR_OK;
}

static int
make_interface(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    char	name[256];
    gqlType	type;

    if (AGOO_ERR_OK != extract_name(err, doc, interface_str, sizeof(interface_str) - 1, name, sizeof(name))) {
	return err->code;
    }
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_interface_create(err, name, desc, len))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	goto ERROR;
    }
    agoo_doc_skip_white(doc);
    if ('{' != *doc->cur) {
	goto ERROR;
    }
    doc->cur++;
    agoo_doc_skip_white(doc);

    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type)) {
	    goto ERROR;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
ERROR:
    if (!x) {
	gql_type_destroy(type);
    }
    return err->code;
}

static int
make_input(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    char	name[256];
    gqlType	type;

    if (AGOO_ERR_OK != extract_name(err, doc, input_str, sizeof(input_str) - 1, name, sizeof(name))) {
	return err->code;
    }
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_input_create(err, name, desc, len))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	goto ERROR;
    }
    agoo_doc_skip_white(doc);
    if ('{' != *doc->cur) {
	agoo_doc_err(doc, err, "Expected '{'");
	goto ERROR;
    }
    doc->cur++;
    agoo_doc_skip_white(doc);

    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_input_arg(err, doc, type)) {
	    goto ERROR;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
ERROR:
    if (!x) {
	gql_type_destroy(type);
    }
    return err->code;
}

static int
extract_interfaces(agooErr err, agooDoc doc, gqlTypeLink *interfacesp) {
    gqlType	type;
    gqlTypeLink	link;
    bool	required;
    bool	first = true;

    agoo_doc_skip_white(doc);
    if (0 != strncmp("implements", doc->cur, 10)) {
	return AGOO_ERR_OK;
    }
    doc->cur += 10;
    if (0 == agoo_doc_skip_white(doc)) {
	return agoo_doc_err(doc, err, "Expected white after 'implements'");
    }
    while (doc->cur < doc->end) {
	agoo_doc_skip_white(doc);
	if ('{' == *doc->cur || '@' == *doc->cur) {
	    break;
	}
	if ('&' == *doc->cur) {
	    doc->cur++;
	    agoo_doc_skip_white(doc);
	} else if (!first) {
	    return agoo_doc_err(doc, err, "Expected &");
	}
	first = false;
	required = false;
	type = NULL;
    	if (AGOO_ERR_OK != read_type(err, doc, &type, &required)) {
	    return err->code;
	}
	if (NULL == (link = (gqlTypeLink)AGOO_MALLOC(sizeof(struct _gqlTypeLink)))) {
	    return AGOO_ERR_MEM(err, "GraphQL Interface");
	}
	link->next = NULL;
	link->type = type;
	if (NULL == *interfacesp) {
	    *interfacesp = link;
	} else {
	    gqlTypeLink	tl = *interfacesp;

	    for (; NULL != tl->next; tl = tl->next) {
	    }
	    tl->next = link;
	}
    }
    return AGOO_ERR_OK;
}

static int
make_type(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    char	name[256];
    gqlType	type;

    if (AGOO_ERR_OK != extract_name(err, doc, type_str, sizeof(type_str) - 1, name, sizeof(name))) {
	return err->code;
    }
    if (x) {
	if (NULL == (type = gql_type_get(name))) {
	    return agoo_doc_err(doc, err, "%s not defined. Can not be extended.", name);
	}
    } else if (NULL == (type = gql_type_create(err, name, desc, len, NULL))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_interfaces(err, doc, &type->interfaces)) {
	goto ERROR;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	goto ERROR;
    }
    agoo_doc_skip_white(doc);
    if ('{' != *doc->cur) {
	agoo_doc_err(doc, err, "Expected '{'");
	goto ERROR;
    }
    doc->cur++;
    agoo_doc_skip_white(doc);

    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type)) {
	    goto ERROR;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
ERROR:
    if (!x) {
	gql_type_destroy(type);
    }
    return err->code;
}

static int
make_schema(agooErr err, agooDoc doc, const char *desc, int len, bool x) {
    gqlType	type;

    if (0 != strncmp(doc->cur, schema_str, sizeof(schema_str) - 1)) {
	return agoo_doc_err(doc, err, "Expected schema key word");
    }
    doc->cur += sizeof(schema_str) - 1;
    if (0 == agoo_doc_skip_white(doc)) {
	return agoo_doc_err(doc, err, "Expected schema key word");
    }
    if (x) {
	if (NULL == (type = gql_type_get(schema_str))) {
	    return agoo_doc_err(doc, err, "schema not defined. Can not be extended.");
	}
    } else if (NULL == (type = gql_schema_create(err, desc, len))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &type->dir)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('{' != *doc->cur) {
	if (!x) {
	    gql_type_destroy(type);
	}
	return agoo_doc_err(doc, err, "Expected '{'");
    }
    doc->cur++;
    agoo_doc_skip_white(doc);
    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type)) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
}

static int
extend(agooErr err, agooDoc doc) {
    if (0 != strncmp(doc->cur, extend_str, sizeof(extend_str) - 1)) {
	return agoo_doc_err(doc, err, "Expected extend key word");
    }
    doc->cur += sizeof(extend_str) - 1;
    if (0 == agoo_doc_skip_white(doc)) {
	return agoo_doc_err(doc, err, "Expected extend key word");
    }
    return AGOO_ERR_OK;
}

int
sdl_parse(agooErr err, const char *str, int len) {
    struct _agooDoc	doc;
    const char		*desc = NULL;
    size_t		dlen = 0;
    bool		extend_next = false;

    agoo_doc_init(&doc, str, len);

    while (doc.cur < doc.end) {
	agoo_doc_next_token(&doc);
	switch (*doc.cur) {
	case '"':
	    if (AGOO_ERR_OK != extract_desc(err, &doc, &desc, &dlen)) {
		return err->code;
	    }
	    break;
	case 's': // schema or scalar
	    if (6 < (doc.end - doc.cur) && 'c' == doc.cur[1]) {
		if ('a' == doc.cur[2]) {
		    if (AGOO_ERR_OK != make_scalar(err, &doc, desc, (int)dlen, extend_next)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		} else {
		    if (AGOO_ERR_OK != make_schema(err, &doc, desc, (int)dlen, extend_next)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		}
	    }
	    return agoo_doc_err(&doc, err, "Unknown directive");
	case 'e': // enum or extend
	    if (4 < (doc.end - doc.cur)) {
		if ('n' == doc.cur[1]) {
		    if (AGOO_ERR_OK != make_enum(err, &doc, desc, (int)dlen, extend_next)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		} else {
		    if (AGOO_ERR_OK != extend(err, &doc)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    extend_next = true;
		    break;
		}
	    }
	    return agoo_doc_err(&doc, err, "Unknown directive");
	case 'u': // union
	    if (AGOO_ERR_OK != make_union(err, &doc, desc, (int)dlen, extend_next)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 'd': // directive
	    if (AGOO_ERR_OK != make_directive(err, &doc, desc, (int)dlen)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 't': // type
	    if (AGOO_ERR_OK != make_type(err, &doc, desc, (int)dlen, extend_next)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 'i': // interface, input
	    if (5 < (doc.end - doc.cur) && 'n' == doc.cur[1]) {
		if ('p' == doc.cur[2]) {
		    if (AGOO_ERR_OK != make_input(err, &doc, desc, (int)dlen, extend_next)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		} else {
		    if (AGOO_ERR_OK != make_interface(err, &doc, desc, (int)dlen, extend_next)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		}
	    }
	    return agoo_doc_err(&doc, err, "Unknown directive");
	case '\0':
	    return AGOO_ERR_OK;
	default:
	    return agoo_doc_err(&doc, err, "Unknown directive");
	}
    }
    return AGOO_ERR_OK;
}

// Parse Execution Definition.

static gqlSelArg
sel_arg_create(agooErr err, const char *name, gqlValue value, gqlVar var) {
    gqlSelArg	arg;

    if (NULL == (arg = (gqlSelArg)AGOO_MALLOC(sizeof(struct _gqlSelArg)))) {
	AGOO_ERR_MEM(err, "GraphQL Selection Field Argument");
    } else {
	arg->next = NULL;
	if (NULL == (arg->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(arg);
	    return NULL;
	}
	arg->var = var;
	arg->value = value;
    }
    return arg;
}
static int
make_sel_arg(agooErr err, agooDoc doc, gqlDoc gdoc, gqlOp op, gqlSel sel) {
    char	name[256];
    gqlValue	value = NULL;
    gqlVar	var = NULL;
    gqlSelArg	arg;

    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected :");
    }
    doc->cur++;
    agoo_doc_skip_white(doc);
    if ('$' == *doc->cur && NULL != op) {
	char	var_name[256];

	doc->cur++;
	if (0 == read_name(err, doc, var_name, sizeof(var_name))) {
	    return err->code;
	}
	for (var = op->vars; NULL != var; var = var->next) {
	    if (0 == strcmp(var_name, var->name)) {
		break;
	    }
	}
	if (NULL == var || NULL == var->value) {
	    for (var = gdoc->vars; NULL != var; var = var->next) {
		if (0 == strcmp(var_name, var->name)) {
		    break;
		}
	    }
	}
	if (NULL == var) {
	    return agoo_doc_err(doc, err, "variable $%s not defined for operation or document", var_name);
	}
    } else if (NULL == (value = agoo_doc_read_value(err, doc, NULL))) {
	return err->code;
    }
    if (NULL == (arg = sel_arg_create(err, name, value, var))) {
	return err->code;
    }
    if (NULL == sel->args) {
	sel->args = arg;
    } else {
	gqlSelArg	a;

	for (a = sel->args; NULL != a->next; a = a->next) {
	}
	a->next = arg;
    }
    return AGOO_ERR_OK;
}

gqlVar
gql_op_var_create(agooErr err, const char *name, gqlType type, gqlValue value) {
    gqlVar	var;

    if (NULL == (var = (gqlVar)AGOO_MALLOC(sizeof(struct _gqlVar)))) {
	AGOO_ERR_MEM(err, "GraphQL Operations Variable");
    } else {
	var->next = NULL;
	if (NULL == (var->name = AGOO_STRDUP(name))) {
	    AGOO_ERR_MEM(err, "strdup()");
	    AGOO_FREE(var);
	    return NULL;
	}
	var->type = type;
	var->value = value;
    }
    return var;
}

static int
make_op_var(agooErr err, agooDoc doc, gqlOp op) {
    char	name[256];
    gqlType	type;
    bool	ignore;
    gqlValue	value = NULL;
    gqlVar	var;

    agoo_doc_skip_white(doc);
    if ('$' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected $");
    }
    doc->cur++;
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if (':' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected :");
    }
    doc->cur++;

    if (AGOO_ERR_OK != read_type(err, doc, &type, &ignore)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);

    if ('=' == *doc->cur) {
	doc->cur++;
	if (NULL == (value = agoo_doc_read_value(err, doc, type))) {
	    return err->code;
	}
    }
    if (NULL == (var = gql_op_var_create(err, name, type, value))) {
	return err->code;
    }
    if (NULL == op->vars) {
	op->vars = var;
    } else {
	gqlVar	v;

	for (v = op->vars; NULL != v->next; v = v->next) {
	}
	v->next = var;
    }
    return AGOO_ERR_OK;
}

static gqlSel
sel_create(agooErr err, const char *alias, const char *name, const char *frag) {
    gqlSel	sel;

    if (NULL == (sel = (gqlSel)AGOO_MALLOC(sizeof(struct _gqlSel)))) {
	AGOO_ERR_MEM(err, "GraphQL Selection Set");
    } else {
	sel->next = NULL;
	if (NULL == name) {
	    sel->name = NULL;
	} else {
	    if (NULL == (sel->name = AGOO_STRDUP(name))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(sel);
		return NULL;
	    }
	}
	if (NULL == alias) {
	    sel->alias = NULL;
	} else {
	    if (NULL == (sel->alias = AGOO_STRDUP(alias))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(sel);
		return NULL;
	    }
	}
	if (NULL == frag) {
	    sel->frag = NULL;
	} else {
	    if (NULL == (sel->frag = AGOO_STRDUP(frag))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(sel);
		return NULL;
	    }
	}
	sel->type = NULL;
	sel->dir = NULL;
    	sel->args = NULL;
	sel->sels = NULL;
	sel->inline_frag = NULL;
    }
    return sel;
}

static gqlSel
make_sel_inline(agooErr err, agooDoc doc, gqlDoc gdoc, gqlOp op) {
    gqlSel	sel = NULL;

    doc->cur += 3;
    agoo_doc_skip_white(doc);
    if ('o' == *doc->cur && 'n' == doc->cur[1]) { // inline fragment
	char	type_name[256];

	doc->cur += 2;
	agoo_doc_skip_white(doc);
	if (0 == read_name(err, doc, type_name, sizeof(type_name))) {
	    return NULL;
	}
	if (NULL == (sel = sel_create(err, NULL, NULL, NULL))) {
	    return NULL;
	}
	if (NULL == (sel->inline_frag = gql_fragment_create(err, NULL, gql_assure_type(err, type_name)))) {
	    return NULL;
	}
	if (AGOO_ERR_OK != extract_dir_use(err, doc, &sel->dir)) {
	    return NULL;
	}
	agoo_doc_skip_white(doc);
	if ('{' == *doc->cur) {
	    doc->cur++;
	    while (doc->cur < doc->end) {
		agoo_doc_skip_white(doc);
		if ('}' == *doc->cur) {
		    doc->cur++;
		    break;
		}
		if (AGOO_ERR_OK != make_sel(err, doc, gdoc, op, &sel->inline_frag->sels)) {
		    return NULL;
		}
	    }
	    if (doc->end <= doc->cur && '}' != doc->cur[-1]) {
		agoo_doc_err(doc, err, "Expected a }");
		return NULL;
	    }
	}
    } else if ('@' == *doc->cur) {
	if (NULL == (sel = sel_create(err, NULL, NULL, NULL))) {
	    return NULL;
	}
	if (NULL == (sel->inline_frag = gql_fragment_create(err, NULL, NULL))) {
	    return NULL;
	}
	if (AGOO_ERR_OK != extract_dir_use(err, doc, &sel->inline_frag->dir)) {
	    return NULL;
	}
	agoo_doc_skip_white(doc);
	if ('{' == *doc->cur) {
	    doc->cur++;
	    while (doc->cur < doc->end) {
		agoo_doc_skip_white(doc);
		if ('}' == *doc->cur) {
		    doc->cur++;
		    break;
		}
		if (AGOO_ERR_OK != make_sel(err, doc, gdoc, op, &sel->inline_frag->sels)) {
		    return NULL;
		}
	    }
	    if (doc->end <= doc->cur && '}' != doc->cur[-1]) {
		agoo_doc_err(doc, err, "Expected a }");
		return NULL;
	    }
	}
    } else { // reference to a fragment
	char	frag_name[256];

	if (0 == read_name(err, doc, frag_name, sizeof(frag_name))) {
	    return NULL;
	}
	sel = sel_create(err, NULL, NULL, frag_name);
    }
    return sel;
}

static int
make_sel(agooErr err, agooDoc doc, gqlDoc gdoc, gqlOp op, gqlSel *parentp) {
    char	alias[256];
    char	name[256];
    gqlSel	sel = NULL;

    if (NULL == op && NULL == parentp) {
	return agoo_doc_err(doc, err, "Fields can only be in a fragment, operation, or another field.");
    }
    *alias = '\0';
    *name = '\0';
    agoo_doc_skip_white(doc);
    if ('.' == *doc->cur && '.' == doc->cur[1] && '.' == doc->cur[2]) {
	if (NULL == (sel = make_sel_inline(err, doc, gdoc, op))) {
	    return err->code;
	}
    } else {
	if (0 == read_name(err, doc, alias, sizeof(alias))) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
	if (':' == *doc->cur) {
	    doc->cur++;
	    agoo_doc_skip_white(doc);
	    if (0 == read_name(err, doc, name, sizeof(name))) {
		return err->code;
	    }
	    agoo_doc_skip_white(doc);
	}

	if ('\0' == *name) { // no alias
	    sel = sel_create(err, NULL, alias, NULL);
	} else {
	    sel = sel_create(err, alias, name, NULL);
	}
    }
    if (NULL == sel) {
	return err->code;
    }
    if (NULL == parentp) {
	if (NULL == op->sels) {
	    op->sels = sel;
	} else {
	    gqlSel	s;

	    for (s = op->sels; NULL != s->next; s = s->next) {
	    }
	    s->next = sel;
	}
    } else {
	if (NULL == *parentp) {
	    *parentp = sel;
	} else {
	    gqlSel	s;

	    for (s = *parentp; NULL != s->next; s = s->next) {
	    }
	    s->next = sel;
	}
    }
    if (NULL == sel->frag && '(' == *doc->cur) {
	doc->cur++;
	while (doc->cur < doc->end) {
	    agoo_doc_skip_white(doc);
	    if (')' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    if (AGOO_ERR_OK != make_sel_arg(err, doc, gdoc, op, sel)) {
		return err->code;
	    }
	}
	agoo_doc_skip_white(doc);
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &sel->dir)) {
	return err->code;
    }
    if (NULL == sel->frag && '{' == *doc->cur) {
	doc->cur++;
	while (doc->cur < doc->end) {
	    agoo_doc_skip_white(doc);
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    if (AGOO_ERR_OK != make_sel(err, doc, gdoc, op, &sel->sels)) {
		return err->code;
	    }
	}
	if (doc->end <= doc->cur && '}' != doc->cur[-1]) {
	    return agoo_doc_err(doc, err, "Expected a }");
	}
    }
    return AGOO_ERR_OK;
}


static int
make_op(agooErr err, agooDoc doc, gqlDoc gdoc, gqlOpKind kind) {
    char	name[256];
    const char	*start;
    gqlOp	op;
    size_t	nlen;

    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    if ( start < doc->cur) {
	if (5 == (doc->cur - start) && 0 == strncmp(query_str, start, sizeof(query_str) - 1)) {
	    kind = GQL_QUERY;
	} else if (8 == (doc->cur - start) && 0 == strncmp(mutation_str, start, sizeof(mutation_str) - 1)) {
	    kind = GQL_MUTATION;
	} else if (12 == (doc->cur - start) && 0 == strncmp(subscription_str, start, sizeof(subscription_str) - 1)) {
	    kind = GQL_SUBSCRIPTION;
	} else {
	    return agoo_doc_err(doc, err, "Invalid operation type");
	}
    }
    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    nlen = doc->cur - start;
    if (sizeof(name) <= nlen) {
	agoo_doc_err(doc, err, "Name too long");
	return err->code;
    }
    if (0 < nlen) {
	strncpy(name, start, nlen);
    }
    name[nlen] = '\0';
    if (NULL == (op = gql_op_create(err, name, kind))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('(' == *doc->cur) {
	doc->cur++;
	while (doc->cur < doc->end) {
	    agoo_doc_skip_white(doc);
	    if (')' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    if (AGOO_ERR_OK != make_op_var(err, doc, op)) {
		return err->code;
	    }
	}
	agoo_doc_skip_white(doc);
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &op->dir)) {
	return err->code;
    }
    if ('{' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected a {");
    }
    doc->cur++;
    while (doc->cur < doc->end) {
	agoo_doc_skip_white(doc);
	if ('}' == *doc->cur) {
	    doc->cur++;
	    break;
	}
	if (AGOO_ERR_OK != make_sel(err, doc, gdoc, op, NULL)) {
	    return err->code;
	}
    }
    if (doc->end <= doc->cur && '}' != doc->cur[-1]) {
	return agoo_doc_err(doc, err, "Expected a }");
    }
    if (NULL == gdoc->ops) {
	gdoc->ops = op;
    } else {
	gqlOp	o;

	for (o = gdoc->ops; NULL != o->next; o = o->next) {
	}
	o->next = op;
    }
    return AGOO_ERR_OK;
}

static int
make_fragment(agooErr err, agooDoc doc, gqlDoc gdoc) {
    char	name[256];
    char	type_name[256];
    const char	*start;
    gqlFrag	frag;

    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    if (8 != (doc->cur - start) || 0 != strncmp("fragment", start, 8)) {
	agoo_doc_err(doc, err, "Expected the key word 'fragment'.");
    }
    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    if (2 != (doc->cur - start) || 0 != strncmp("on", start, 2)) {
	agoo_doc_err(doc, err, "Expected the key word 'on'.");
    }
    agoo_doc_skip_white(doc);
    if (0 == read_name(err, doc, type_name, sizeof(type_name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);

    if (NULL == (frag = gql_fragment_create(err, name, gql_assure_type(err, type_name)))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &frag->dir)) {
	return err->code;
    }
    if ('{' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected a {");
    }
    doc->cur++;

    while (doc->cur < doc->end) {
	agoo_doc_skip_white(doc);
	if ('}' == *doc->cur) {
	    doc->cur++;
	    break;
	}
	if (AGOO_ERR_OK != make_sel(err, doc, gdoc, NULL, &frag->sels)) {
	    return err->code;
	}
    }
    if (doc->end <= doc->cur  && '}' != doc->cur[-1]) {
	return agoo_doc_err(doc, err, "Expected a }");
    }
    if (NULL == gdoc->frags) {
	gdoc->frags = frag;
    } else {
	gqlFrag	f;

	for (f = gdoc->frags; NULL != f->next; f = f->next) {
	}
	f->next = frag;
    }
    return AGOO_ERR_OK;
}

static gqlType
lookup_field_type(gqlType type, const char *field, bool qroot) {
    gqlType	ftype = NULL;

    switch (type->kind) {
    case GQL_SCHEMA:
    case GQL_OBJECT:
    case GQL_INPUT:
    case GQL_INTERFACE: {
	gqlField	f;

	for (f = type->fields; NULL != f; f = f->next) {
	    if (0 == strcmp(field, f->name)) {
		ftype = f->type;
		break;
	    }
	}
	if (NULL == ftype) {
	    if (0 == strcmp("__typename", field)) {
		ftype = &gql_string_type;
	    } else if (qroot) {
		if (0 == strcmp("__type", field)) {
		    ftype = gql_type_get("__Type");
		} else if (0 == strcmp("__schema", field)) {
		    ftype = gql_type_get("__Schema");
		}
	    }
	}
	break;
    }
    case GQL_LIST:
	ftype = lookup_field_type(type->base, field, false);
	break;
    case GQL_UNION: // Can not be used directly for query type determinations.
    default:
	break;
    }
    return ftype;
}

static int
sel_set_type(agooErr err, gqlType type, gqlSel sels, bool qroot) {
    gqlSel	sel;

    for (sel = sels; NULL != sel; sel = sel->next) {
	if (NULL == sel->name) { // inline or fragment
	    sel->type = type;
	    if (NULL != sel->inline_frag) {
		gqlType	ftype = type;

		if (NULL != sel->inline_frag->on) {
		    ftype = sel->inline_frag->on;
		}
		if (AGOO_ERR_OK != sel_set_type(err, ftype, sel->inline_frag->sels, false)) {
		    return err->code;
		}
	    }
	} else {
	    if (NULL == (sel->type = lookup_field_type(type, sel->name, qroot))) {
		return agoo_err_set(err, AGOO_ERR_EVAL, "Failed to determine the type for %s.", sel->name);
	    }
	}
	if (NULL != sel->sels) {
	    if (AGOO_ERR_OK != sel_set_type(err, sel->type, sel->sels, false)) {
		return err->code;
	    }
	}
    }
    return AGOO_ERR_OK;
}

static int
validate_doc(agooErr err, gqlDoc doc) {
    gqlOp	op;
    gqlType	schema;
    gqlType	type = NULL;
    gqlFrag	frag;
    int		cnt;

    if (NULL == (schema = gql_root_type())) {
	return agoo_err_set(err, AGOO_ERR_EVAL, "No root (schema) type defined.");
    }
    for (frag = doc->frags; NULL != frag; frag = frag->next) {
	if (AGOO_ERR_OK != sel_set_type(err, frag->on, frag->sels, false)) {
	    return err->code;
	}
    }
    cnt = 0;
    for (op = doc->ops; NULL != op; op = op->next) {
	if (NULL == op->name) {
	    cnt++;
	    if (1 < cnt) {
		return agoo_err_set(err, AGOO_ERR_EVAL, "Multiple un-named operation.");
	    }
	} else {
	    gqlOp	o2 = op->next;

	    for (; NULL != o2; o2 = o2->next) {
		if (NULL == o2->name) {
		    continue;
		}
		if (0 == strcmp(o2->name, op->name)) {
		    return agoo_err_set(err, AGOO_ERR_EVAL, "Multiple operation named '%s'.", op->name);
		}
	    }
	}
    }
    for (op = doc->ops; NULL != op; op = op->next) {
	switch (op->kind) {
	case GQL_QUERY:
	    type = lookup_field_type(schema, query_str, false);
	    break;
	case GQL_MUTATION:
	    type = lookup_field_type(schema, mutation_str, false);
	    break;
	case GQL_SUBSCRIPTION:
	    type = lookup_field_type(schema, subscription_str, false);
	    break;
	default:
	    break;
	}
	if (NULL == type) {
	    return agoo_err_set(err, AGOO_ERR_EVAL, "Not a supported operation type.");
	}
	if (AGOO_ERR_OK != sel_set_type(err, type, op->sels, GQL_QUERY == op->kind)) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

gqlDoc
sdl_parse_doc(agooErr err, const char *str, int len, gqlVar vars, gqlOpKind default_kind) {
    struct _agooDoc	doc;
    gqlDoc		gdoc = NULL;

    agoo_doc_init(&doc, str, len);

    if (NULL == (gdoc = gql_doc_create(err))) {
	return NULL;
    }
    gdoc->vars = vars;
    while (doc.cur < doc.end) {
	agoo_doc_next_token(&doc);
	if (doc.end <= doc.cur) {
	    break;
	}
	switch (*doc.cur) {
	case '{': // no name query
	case 'q':
	case 'm':
	case 's':
	    if (AGOO_ERR_OK != make_op(err, &doc, gdoc, default_kind)) {
		gql_doc_destroy(gdoc);
		return NULL;
	    }
	    break;
	case 'f':
	    if (AGOO_ERR_OK != make_fragment(err, &doc, gdoc)) {
		gql_doc_destroy(gdoc);
		return NULL;
	    }
	    break;
	case '\0':
	    goto DONE;
	default:
	    agoo_doc_err(&doc, err, "unexpected character");
	    gql_doc_destroy(gdoc);
	    return NULL;
	}
    }
DONE:
    if (AGOO_ERR_OK != validate_doc(err, gdoc)) {
	gql_doc_destroy(gdoc);
	return NULL;
    }
    return gdoc;
}
