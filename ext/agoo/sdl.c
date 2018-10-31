// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "doc.h"
#include "graphql.h"
#include "sdl.h"

static int
make_scalar(Err err, Doc doc, const char *desc, int len) {
    const char	*start;
    char	name[256];
    size_t	nlen;

    if (0 != strncmp(doc->cur, "scalar", 6)) {
	return doc_err(doc, err, "Expected scalar key word");
    }
    doc->cur += 6;
    if (0 == doc_skip_white(doc)) {
	return doc_err(doc, err, "Expected scalar key word");
    }
    start = doc->cur;
    doc_read_token(doc);

    if (doc->cur == start) {
	return doc_err(doc, err, "Scalar name not provided");
    }
    nlen = doc->cur - start;
    if (sizeof(name) <= nlen) {
	return doc_err(doc, err, "Scalar name too long");
    }
    strncpy(name, start, nlen);
    name[nlen] = '\0';

    gql_scalar_create(err, name, desc, len, false);
    
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
	    desc_end = doc.cur - 1; // TBD handl triples also
	    break;
	case 's': // scalar
	    if (ERR_OK != make_scalar(err, &doc, desc, (int)(desc_end - desc))) {
		return err->code;
	    }
	    break;
	case '\0':
	    return ERR_OK;
	default:
	    printf("*** ?? %02x '%s'\n", *doc.cur, doc.cur);
	    return ERR_OK;
	    break;
	}
    }
    // TBD
    
    return ERR_OK;
}
