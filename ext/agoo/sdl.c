// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "doc.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sdl.h"

static int
extract_name(agooErr err, agooDoc doc, const char *key, int klen, char *name, size_t max) {
    const char	*start;
    size_t	nlen;

    if (0 != strncmp(doc->cur, key, klen)) {
	return doc_err(doc, err, "Expected %s key word", key);
    }
    doc->cur += klen;
    if (0 == doc_skip_white(doc)) {
	return doc_err(doc, err, "Expected %s key word", key);
    }
    start = doc->cur;
    doc_read_token(doc);
    if (doc->cur == start) {
	return doc_err(doc, err, "Name not provided");
    }
    nlen = doc->cur - start;
    if (max <= nlen) {
	return doc_err(doc, err, "Name too long");
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';
    
    return ERR_OK;
}

static int
make_scalar(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];

    if (ERR_OK != extract_name(err, doc, "scalar", 6, name, sizeof(name))) {
	return err->code;
    }
    gql_scalar_create(err, name, desc, len, false);
    
    return ERR_OK;
}

static int
make_enum(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlType	type;
    
    if (ERR_OK != extract_name(err, doc, "enum", 4, name, sizeof(name))) {
	return err->code;
    }
    doc_skip_white(doc);
    if ('{' != *doc->cur) {
	return doc_err(doc, err, "Expected '{'");
    }
    doc->cur++;

    if (NULL == (type = gql_enum_create(err, name, desc, len, false))) {
	return err->code;
    }
    while (doc->cur < doc->end) {
	doc_skip_white(doc);
	start = doc->cur;
	doc_read_token(doc);
	if (doc->cur == start) {
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    return doc_err(doc, err, "Invalid Enum value");
	}
	if (ERR_OK != gql_enum_add(err, type, start, (int)(doc->cur - start))) {
	    return err->code;
	}
    }
    return ERR_OK;
}

static int
make_union(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlType	type;
    
    if (ERR_OK != extract_name(err, doc, "union", 5, name, sizeof(name))) {
	return err->code;
    }
    doc_skip_white(doc);
    if ('=' != *doc->cur) {
	return doc_err(doc, err, "Expected '='");
    }
    doc->cur++;
    doc_skip_white(doc);

    if (NULL == (type = gql_union_create(err, name, desc, len, false))) {
	return err->code;
    }
    while (doc->cur < doc->end) {
	doc_skip_white(doc);
	start = doc->cur;
	doc_read_token(doc);
	if (ERR_OK != gql_union_add(err, type, start, (int)(doc->cur - start))) {
	    return err->code;
	}
	doc_skip_white(doc);
	if ('|' != *doc->cur) {
	    break;
	}
	doc->cur++; // skip |
    }
    return ERR_OK;
}

static int
make_arg(agooErr err, agooDoc doc, gqlDir dir) {
    char	name[256];
    char	type_name[256];
    const char	*start;
    const char	*desc = NULL;
    const char	*desc_end = NULL;
    size_t	nlen;
    bool	required = false;
    gqlValue	dv = NULL;
    
    doc_skip_white(doc);
    if ('"' == *doc->cur) {
	desc = doc->cur + 1;
	if (ERR_OK != doc_read_string(err, doc)) {
	    return err->code;
	}
	if ('"' == *desc) { // must be a """
	    desc += 2;
	    desc_end = doc->cur - 3;
	} else {
	    desc_end = doc->cur - 1;
	}
    }
    doc_skip_white(doc);
    start = doc->cur;
    doc_read_token(doc);
    if (doc->cur == start) {
	return doc_err(doc, err, "Argument name not provided");
    }
    if (':' != *doc->cur) {
	return doc_err(doc, err, "Expected ':'");
    }
    nlen = doc->cur - start;
    if (sizeof(name) <= nlen) {
	return doc_err(doc, err, "Name too long");
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';
    doc->cur++;

    // read type
    doc_skip_white(doc);
    start = doc->cur;
    doc_read_token(doc);
    if (doc->cur == start) {
	return doc_err(doc, err, "Argument type not provided");
    }
    nlen = doc->cur - start;
    if (sizeof(type_name) <= nlen) {
	return doc_err(doc, err, "Type name too long");
    }
    strncpy(type_name, start, nlen);
    type_name[nlen] = '\0';

    doc_skip_white(doc);
    if ('!' == *doc->cur) {
	required = true;
    } else if ('=' == *doc->cur) {
	if (NULL == (dv = doc_read_value(err, doc))) {
	    return err->code;
	}
    }
    doc_skip_white(doc);
    if ('@' == *doc->cur) {
	// TBD directive
    }
    if (NULL == gql_dir_arg(err, dir, name, type_name, desc, (int)(desc_end - desc), dv, required)) {
	return err->code;
    }
    return ERR_OK;
}

static int
make_directive(agooErr err, agooDoc doc, const char *desc, int len) {
    char	name[256];
    const char	*start;
    gqlDir	dir;
    size_t	nlen;

    if (0 != strncmp(doc->cur, "directive", 9)) {
	return doc_err(doc, err, "Expected directive key word");
    }
    doc->cur += 9;
    if (0 == doc_skip_white(doc)) {
	return doc_err(doc, err, "Expected directive key word");
    }
    if ('@' != *doc->cur) {
	return doc_err(doc, err, "Expected '@'");
    }
    doc->cur++;
    start = doc->cur;
    doc_read_token(doc);
    if (doc->cur == start) {
	return doc_err(doc, err, "Name not provided");
    }
    nlen = doc->cur - start;
    if (sizeof(name) <= nlen) {
	return doc_err(doc, err, "Name too long");
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';
    if (NULL == (dir = gql_directive_create(err, name, desc, len, false))) {
	return err->code;
    }
    if ('(' == *doc->cur) {
	while (doc->cur < doc->end) {
	    if (ERR_OK != make_arg(err, doc, dir)) {
		return err->code;
	    }
	    doc_skip_white(doc);
	    if (')' == *doc->cur) {
		doc->cur++;
		break;
	    }
	}
    }
    doc_skip_white(doc);
    start = doc->cur;
    doc_read_token(doc);

    if (2 != doc->cur - start || 0 != strncmp(start, "on", 2)) {
	return doc_err(doc, err, "Expected 'on'");
    }
    while (doc->cur < doc->end) {
	doc_skip_white(doc);
	start = doc->cur;
	doc_read_token(doc);
	if (ERR_OK != gql_directive_on(err, dir, start, (int)(doc->cur - start))) {
	    return err->code;
	}
	doc_skip_white(doc);
	if ('|' != *doc->cur) {
	    break;
	}
	doc->cur++; // skip |
    }
    return ERR_OK;
}

int
sdl_parse(agooErr err, const char *str, int len) {
    struct _agooDoc	doc;
    const char		*desc = NULL;
    const char		*desc_end = NULL;
    
    doc_init(&doc, str, len);

    while (doc.cur < doc.end) {
	doc_next_token(&doc);
	switch (*doc.cur) {
	case '"':
	    desc = doc.cur + 1;
	    if (ERR_OK != doc_read_string(err, &doc)) {
		return err->code;
	    }
	    if ('"' == *desc) { // must be a """
		desc += 2;
		desc_end = doc.cur - 3;
	    } else {
		desc_end = doc.cur - 1;
	    }
	    break;
	case 's': // scalar
	    if (ERR_OK != make_scalar(err, &doc, desc, (int)(desc_end - desc))) {
		return err->code;
	    }
	    break;
	case 'e': // enum, and extend interface or type
	    if (4 < (doc.end - doc.cur)) {
		if ('n' == doc.cur[1]) {
		    if (ERR_OK != make_enum(err, &doc, desc, (int)(desc_end - desc))) {
			return err->code;
		    }
		    break;
		} else {
		    // TBD extend
		    break;
		}
	    }
	    return doc_err(&doc, err, "Unknown directive");
	case 'u': // union
	    if (ERR_OK != make_union(err, &doc, desc, (int)(desc_end - desc))) {
		return err->code;
	    }
	    break;
	case 'd': // directive
	    if (ERR_OK != make_directive(err, &doc, desc, (int)(desc_end - desc))) {
		return err->code;
	    }
	    break;
	case 't': // type
	    break;
	case 'i': // interface, input
	    if (5 < (doc.end - doc.cur) && 'n' == doc.cur[1]) {
		if ('p' == doc.cur[2]) {
		    // input
		    break;
		} else {
		    // TBD interface
		    break;
		}
	    }
	    return doc_err(&doc, err, "Unknown directive");
	case 'f': // fragment
	    break;
	case '\0':
	    return ERR_OK;
	default:
	    return doc_err(&doc, err, "Unknown directive");
	}
    }
    return ERR_OK;
}
