// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "doc.h"
#include "graphql.h"
#include "sdl.h"

static int
extract_name(Err err, Doc doc, const char *key, int klen, char *name, size_t max) {
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
make_scalar(Err err, Doc doc, const char *desc, int len) {
    char	name[256];

    if (ERR_OK != extract_name(err, doc, "scalar", 6, name, sizeof(name))) {
	return err->code;
    }
    gql_scalar_create(err, name, desc, len, false);
    
    return ERR_OK;
}

static int
make_enum(Err err, Doc doc, const char *desc, int len) {
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
	if (ERR_OK != gql_enum_add(err, type, start, doc->cur - start)) {
	    return err->code;
	}
    }
    return ERR_OK;
}

int
sdl_parse(Err err, const char *str, int len) {
    struct _Doc	doc;
    const char	*desc = NULL;
    const char	*desc_end = NULL;
    
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
	    break;
	case 'd': // directive
	    // TBD maybe keep a list of directives
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
