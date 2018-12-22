// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "doc.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

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
	if ('"' == *desc) { // must be a """
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
make_scalar(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];

    if (AGOO_ERR_OK != extract_name(err, doc, "scalar", 6, name, sizeof(name))) {
	return err->code;
    }
    gql_scalar_create(err, name, desc, len);
    
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
make_enum(agooErr err, agooDoc doc, const char *desc, size_t dlen) {
    char	name[256];
    const char	*start;
    gqlType	type;
    gqlDirUse	uses = NULL;
    gqlEnumVal	ev;
    size_t	len;
    
    if (AGOO_ERR_OK != extract_name(err, doc, "enum", 4, name, sizeof(name))) {
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

    if (NULL == (type = gql_enum_create(err, name, desc, dlen))) {
	return err->code;
    }
    type->dir = uses;
    while (doc->cur < doc->end) {
	desc = NULL;
	uses = NULL;

	if (AGOO_ERR_OK != extract_desc(err, doc, &desc, &dlen)) {
	    return err->code;
	}
	start = doc->cur;
	agoo_doc_read_token(doc);
	len = doc->cur - start;
	if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	    return err->code;
	}
	if (doc->cur == start) {
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    return agoo_doc_err(doc, err, "Invalid Enum value");
	}
	if (NULL == (ev = gql_enum_append(err, type, start, len, desc, dlen))) {
	    return err->code;
	}
	ev->dir = uses;
    }
    return AGOO_ERR_OK;
}

static int
make_union(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    gqlType	type;
    gqlDirUse	uses = NULL;
    gqlType	member;
    bool	required;
    
    if (AGOO_ERR_OK != extract_name(err, doc, "union", 5, name, sizeof(name))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    if ('=' != *doc->cur) {
	return agoo_doc_err(doc, err, "Expected '='");
    }
    doc->cur++;
    agoo_doc_skip_white(doc);

    if (NULL == (type = gql_union_create(err, name, desc, len))) {
	return err->code;
    }
    type->dir = uses;
    while (doc->cur < doc->end) {
	if (AGOO_ERR_OK != read_type(err, doc, &member, &required)) {
	    return err->code;
	}
 	if (AGOO_ERR_OK != gql_union_add(err, type, member)) {
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
make_field(agooErr err, agooDoc doc, gqlType type, bool allow_args) {
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
    if (0 == read_name(err, doc, name, sizeof(name))) {
	return err->code;
    }
    agoo_doc_skip_white(doc);
    switch (*doc->cur) {
    case '(':
	if (!allow_args) {
	    return agoo_doc_err(doc, err, "Input fields can not have arguments.");
	}
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
    if (NULL == (field = gql_type_field(err, type, name, return_type, dval, desc, dlen, required, NULL))) {
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
make_interface(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    gqlType	type;
    gqlDirUse	uses = NULL;
    
    if (AGOO_ERR_OK != extract_name(err, doc, "interface", 9, name, sizeof(name))) {
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
    agoo_doc_skip_white(doc);

    if (NULL == (type = gql_interface_create(err, name, desc, len))) {
	return err->code;
    }
    type->dir = uses;
    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type, true)) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
}

static int
make_input(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    gqlType	type;
    gqlDirUse	uses = NULL;

    if (AGOO_ERR_OK != extract_name(err, doc, "input", 5, name, sizeof(name))) {
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
    agoo_doc_skip_white(doc);

    if (NULL == (type = gql_input_create(err, name, desc, len))) {
	return err->code;
    }
    type->dir = uses;

    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type, false)) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
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
	    return agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a GraphQL interface.");
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
make_type(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    gqlType	type;
    gqlDirUse	uses = NULL;
    gqlTypeLink	interfaces = NULL;

    if (AGOO_ERR_OK != extract_name(err, doc, "type", 4, name, sizeof(name))) {
	return err->code;
    }
    if (AGOO_ERR_OK != extract_interfaces(err, doc, &interfaces)) {
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
    agoo_doc_skip_white(doc);

    if (NULL == (type = gql_type_create(err, name, desc, len, interfaces))) {
	return err->code;
    }
    type->dir = uses;

    while (doc->cur < doc->end) {
	if ('}' == *doc->cur) {
	    doc->cur++; // skip }
	    break;
	}
	if (AGOO_ERR_OK != make_field(err, doc, type, true)) {
	    return err->code;
	}
	agoo_doc_skip_white(doc);
    }
    return AGOO_ERR_OK;
}

int
sdl_parse(agooErr err, const char *str, int len) {
    struct _agooDoc	doc;
    const char		*desc = NULL;
    size_t		dlen = 0;
    
    agoo_doc_init(&doc, str, len);

    while (doc.cur < doc.end) {
	agoo_doc_next_token(&doc);
	switch (*doc.cur) {
	case '"':
	    if (AGOO_ERR_OK != extract_desc(err, &doc, &desc, &dlen)) {
		return err->code;
	    }
	    break;
	case 's': // scalar
	    if (AGOO_ERR_OK != make_scalar(err, &doc, desc, dlen)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 'e': // enum, and extend interface or type
	    if (4 < (doc.end - doc.cur)) {
		if ('n' == doc.cur[1]) {
		    if (AGOO_ERR_OK != make_enum(err, &doc, desc, dlen)) {
			return err->code;
		    }
		    desc = NULL;
		    dlen = 0;
		    break;
		} else {
		    // TBD extend
		    desc = NULL;
		    dlen = 0;
		    break;
		}
	    }
	    return agoo_doc_err(&doc, err, "Unknown directive");
	case 'u': // union
	    if (AGOO_ERR_OK != make_union(err, &doc, desc, dlen)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 'd': // directive
	    if (AGOO_ERR_OK != make_directive(err, &doc, desc, dlen)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 't': // type
	    if (AGOO_ERR_OK != make_type(err, &doc, desc, dlen)) {
		return err->code;
	    }
	    desc = NULL;
	    dlen = 0;
	    break;
	case 'i': // interface, input
	    if (5 < (doc.end - doc.cur) && 'n' == doc.cur[1]) {
		if ('p' == doc.cur[2]) {
		    if (AGOO_ERR_OK != make_input(err, &doc, desc, dlen)) {
			return err->code;
		    }	
		    desc = NULL;
		    dlen = 0;
		    break;
		} else {
		    if (AGOO_ERR_OK != make_interface(err, &doc, desc, dlen)) {
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
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a selection field argument.");
    } else {
	arg->next = NULL;
	if (NULL == (arg->name = strdup(name))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of field name failed. %s:%d", __FILE__, __LINE__);
	    return NULL;
	}
	AGOO_ALLOC(arg->name, strlen(arg->name));
	arg->var = var;
	arg->value = value;
    }
    return arg;
}
static int
make_sel_arg(agooErr err, agooDoc doc, gqlOp op, gqlSel sel) {
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
    if ('$' == *doc->cur) {
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
	if (NULL == var) {
	    return agoo_doc_err(doc, err, "variable $%s not defined for operation", var_name);
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

static gqlVar
op_var_create(agooErr err, const char *name, gqlType type, gqlValue value) {
    gqlVar	var;

    if (NULL == (var = (gqlVar)AGOO_MALLOC(sizeof(struct _gqlVar)))) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a operation variable.");
    } else {
	var->next = NULL;
	if (NULL == (var->name = strdup(name))) {
	    agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of variable name failed. %s:%d", __FILE__, __LINE__);
	    return NULL;
	}
	AGOO_ALLOC(var->name, strlen(var->name));
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
    if (NULL == (var = op_var_create(err, name, type, value))) {
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
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a selection set.");
    } else {
	sel->next = NULL;
	if (NULL == name) {
	    sel->name = NULL;
	} else {
	    if (NULL == (sel->name = strdup(name))) {
		agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of selection name failed. %s:%d", __FILE__, __LINE__);
		return NULL;
	    }
	    AGOO_ALLOC(sel->name, strlen(sel->name));
	}
	if (NULL == alias) {
	    sel->alias = NULL;
	} else {
	    if (NULL == (sel->alias = strdup(alias))) {
		agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of selection alias failed. %s:%d", __FILE__, __LINE__);
		return NULL;
	    }
	    AGOO_ALLOC(sel->alias, strlen(sel->alias));
	}
	if (NULL == frag) {
	    sel->frag = NULL;
	} else {
	    if (NULL == (sel->frag = strdup(frag))) {
		agoo_err_set(err, AGOO_ERR_MEMORY, "strdup of selection fragment failed. %s:%d", __FILE__, __LINE__);
		return NULL;
	    }
	    AGOO_ALLOC(sel->frag, strlen(sel->frag));
	}
	sel->dir = NULL;
    	sel->args = NULL;
	sel->sels = NULL;
	sel->inline_frag = NULL;
    }
    return sel;
}    

static int
make_sel(agooErr err, agooDoc doc, gqlOp op, gqlSel parent) {
    char	alias[256];
    char	name[256];
    gqlSel	sel = NULL;
    
    *alias = '\0';
    *name = '\0';
    agoo_doc_skip_white(doc);
    if ('.' == *doc->cur && '.' == doc->cur[1] && '.' == doc->cur[2]) {
	doc->cur += 3;
	agoo_doc_skip_white(doc);
	if ('o' == *doc->cur && 'n' == doc->cur[1]) { // inline fragment
	    // TBD if starts with ... then a fragment name ... fragmentName directives
	} else if ('@' == *doc->cur) {
	    // TBD inline with directive
	} else { // reference to a fragment
	    if (0 == read_name(err, doc, alias, sizeof(alias))) {
		return err->code;
	    }
	    sel = sel_create(err, NULL, NULL, alias);
	}
	// TBD if starts with ... then a fragment name ... fragmentName directives
	//     if starts with ... on typeName then inline fragment, expect directives and sels
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
    if (NULL == parent) {
	if (NULL == op->sels) {
	    op->sels = sel;
	} else {
	    gqlSel	s;

	    for (s = op->sels; NULL != s->next; s = s->next) {
	    }
	    s->next = sel;
	}
    } else {
	if (NULL == parent->sels) {
	    parent->sels = sel;
	} else {
	    gqlSel	s;

	    for (s = parent->sels; NULL != s->next; s = s->next) {
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
	    if (AGOO_ERR_OK != make_sel_arg(err, doc, op, sel)) {
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
	    if (AGOO_ERR_OK != make_sel(err, doc, op, sel)) {
		return err->code;
	    }
	}
	if (doc->end <= doc->cur) {
	    return agoo_doc_err(doc, err, "Expected a }");
	}
    }
    return AGOO_ERR_OK;
}


static int
make_op(agooErr err, agooDoc doc, gqlDoc gdoc) {
    char	name[256];
    const char	*start;
    gqlOpKind	kind;
    gqlOp	op;
    size_t	nlen;

    agoo_doc_skip_white(doc);
    start = doc->cur;
    agoo_doc_read_token(doc);
    if (doc->cur == start ||
	       (5 == (doc->cur - start) && 0 == strncmp("query", start, 5))) {
	kind = GQL_QUERY;
    } else if (8 == (doc->cur - start) && 0 == strncmp("mutation", start, 8)) {
	kind = GQL_MUTATION;
    } else if (12 == (doc->cur - start) && 0 == strncmp("subscription", start, 12)) {
	kind = GQL_SUBSCRIPTION;
    } else {
	return agoo_doc_err(doc, err, "Invalid operation type");
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
	if (AGOO_ERR_OK != make_sel(err, doc, op, NULL)) {
	    return err->code;
	}
    }
    if (doc->end <= doc->cur) {
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

gqlDoc
sdl_parse_doc(agooErr err, const char *str, int len) {
    struct _agooDoc	doc;
    gqlDoc		gdoc = NULL;
    
    agoo_doc_init(&doc, str, len);
    if (NULL == (gdoc = gql_doc_create(err))) {
	return NULL;
    }
    while (doc.cur < doc.end) {
	agoo_doc_next_token(&doc);
	switch (*doc.cur) {
	case '{': // no name query
	case 'q':
	case 'm':
	case 's':
	    if (AGOO_ERR_OK != make_op(err, &doc, gdoc)) {
		return NULL;
	    }
	    break;
	case 'f':
	    // TBD fragment
	    break;
	case '\0':
	    goto DONE;
	default:
	    agoo_doc_err(&doc, err, "unexpected character");
	    return NULL;
	}
    }
DONE:
    return gdoc;
}
