// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "doc.h"
#include "gqlvalue.h"

static char	char_map[256] = "\
.........ww..w..................\
wpq.....pp..w.p.ttttttttttp..p..\
pttttttttttttttttttttttttttp.p.t\
.ttttttttttttttttttttttttttppp..\
................................\
................................\
................................\
................................";

void
agoo_doc_init(agooDoc doc, const char *str, int len) {
    if (0 >= len) {
	len = (int)strlen(str);
    }
    doc->str = str;
    doc->cur = str;
    doc->end = str + len;
    // If there is a BOM, skip it.
    if (0xEF == (uint8_t)*doc->str && 0xBB == (uint8_t)doc->str[1] && 0xBF == (uint8_t)doc->str[2]) {
	doc->cur += 3;
    }
}

int
agoo_doc_skip_white(agooDoc doc) {
    const char	*start = doc->cur;
    
    for (; 'w' == char_map[*(uint8_t*)doc->cur]; doc->cur++) {
    }
    return (int)(doc->cur - start);
}

void
agoo_doc_skip_comment(agooDoc doc) {
    for (; true; doc->cur++) {
	switch (*doc->cur) {
	case '\r':
	case '\n':
	    return;
	default:
	    break;
	}
    }
}

void
agoo_doc_read_token(agooDoc doc) {
    if ('t' == char_map[*(uint8_t*)doc->cur] && '9' < *doc->cur) {
	doc->cur++;
	for (; 't' == char_map[*(uint8_t*)doc->cur]; doc->cur++) {
	}
    }
}

void
agoo_doc_next_token(agooDoc doc) {
    while (true) {
	agoo_doc_skip_white(doc);
	if ('#' == *doc->cur) {
	    agoo_doc_skip_comment(doc);
	} else {
	    break;
	}
    }
}

// Just find end.
int
agoo_doc_read_string(agooErr err, agooDoc doc) {
    doc->cur++; // skip first "
    if ('"' == *doc->cur) { // a """ string or an empty string
	doc->cur++;
	if ('"' != *doc->cur) {
	    return AGOO_ERR_OK; // empty string
	}
	doc->cur++;
	for (; doc->cur < doc->end; doc->cur++) {
	    if ('"' == *doc->cur && '"' == doc->cur[1] && '"' == doc->cur[2]) {
		doc->cur += 3;
		break;
	    }
	}
    } else {
	for (; doc->cur < doc->end; doc->cur++) {
	    if ('"' == *doc->cur) {
		if ('\\' != doc->cur[-1]) {
		    doc->cur++;
		    break;
		}
	    }
	}
    }
    if (doc->end <= doc->cur) {
	return agoo_doc_err(doc, err, "String not terminated");
    }
    return AGOO_ERR_OK;
}

int
agoo_doc_err(agooDoc doc, agooErr err, const char *fmt, ...) {
    va_list	ap;
    char	msg[248];
    int		line = 0;
    int		col = 0;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    agoo_doc_location(doc, &line, &col);
    va_end(ap);

    return agoo_err_set(err, AGOO_ERR_PARSE, "%s at %d:%d", msg, line, col);
}

void
agoo_doc_location(agooDoc doc, int *linep, int *colp) {
    const char	*s;
    int		line = 1;
    int		col = 1;

    for (s = doc->str; s < doc->cur; s++) {
	switch (*s) {
	case '\n':
	    line++;
	    col = 1;
	    break;
	case '\r':
	    if ('\n' == *(s + 1)) {
		s++;
	    }
	    line++;
	    col = 1;
	    break;
	default:
	    col++;
	    break;
	}
    }
    *linep = line;
    *colp = col;
}

gqlValue
agoo_doc_read_value(agooErr err, agooDoc doc) {
    // TBD handle list and object as well as scalars, object is typeless
    // TBD put this in agoo_doc_read_value
    return NULL;
}

