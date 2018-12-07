// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

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
    if (NULL == (value = agoo_doc_read_value(err, doc))) {
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
make_enum(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlType	type;
    gqlDirUse	uses = NULL;
    
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

    if (NULL == (type = gql_enum_create(err, name, desc, len))) {
	return err->code;
    }
    type->dir = uses;
    while (doc->cur < doc->end) {
	agoo_doc_skip_white(doc);
	// TBD read desc, enum values will have to be more than a string
	start = doc->cur;
	agoo_doc_read_token(doc);

	// TBD extract_dir_use(agooErr err, agooDoc doc, gqlDirUse *uses) {

	if (doc->cur == start) {
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    return agoo_doc_err(doc, err, "Invalid Enum value");
	}
	if (AGOO_ERR_OK != gql_enum_add(err, type, start, (int)(doc->cur - start))) {
	    return err->code;
	}
    }
    return AGOO_ERR_OK;
}

static int
make_union(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlType	type;
    gqlDirUse	uses = NULL;
    
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
	agoo_doc_skip_white(doc);
	start = doc->cur;
	agoo_doc_read_token(doc);
	if (AGOO_ERR_OK != gql_union_add(err, type, start, (int)(doc->cur - start))) {
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

    agoo_doc_skip_white(doc);
    if ('!' == *doc->cur) {
	required = true;
	doc->cur++;
    } else if ('=' == *doc->cur) {
	if (NULL == (dv = agoo_doc_read_value(err, doc))) {
	    return err->code;
	}
    }
    agoo_doc_skip_white(doc);
    if ('@' == *doc->cur) {
	// TBD directive
    }
    if (NULL == (type = gql_assure_type(err, type_name))) {
	return err->code;
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
	if (NULL == (dval = agoo_doc_read_value(err, doc))) {
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
    if (AGOO_ERR_OK != extract_dir_use(err, doc, &uses)) {
	return err->code;
    }
    if (NULL == (field = gql_type_field(err, type, name, return_type, desc, dlen, required, NULL))) {
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
	if (AGOO_ERR_OK != make_field(err, doc, type)) {
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
	    // TBD
	    break;
	case 'i': // interface, input
	    if (5 < (doc.end - doc.cur) && 'n' == doc.cur[1]) {
		if ('p' == doc.cur[2]) {
		    // TBD input
		    desc = NULL;
		    dlen = 0;
		    //break;
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
