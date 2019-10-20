// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "doc.h"
#include "gqlvalue.h"
#include "graphql.h"

#define EXP_MAX		100000
#define DEC_MAX		16

static char	char_map[256] = "\
.........ww..w..................\
wpq.....pp..w.p.ttttttttttp..p..\
pttttttttttttttttttttttttttp.p.t\
.ttttttttttttttttttttttttttppp..\
................................\
................................\
................................\
................................";

static char	json_map[256] = "\
.........ww..w..................\
wpq.....pp..c.p.ttttttttttp..p..\
pttttttttttttttttttttttttttp.p.t\
.ttttttttttttttttttttttttttppp..\
................................\
................................\
................................\
................................";

static char	value_map[256] = "\
.........ww..w..................\
wpq.....pp..ctt.ttttttttttt..p..\
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

    while (true) {
	for (; 'w' == char_map[*(uint8_t*)doc->cur]; doc->cur++) {
	}
	if ('#' == *doc->cur) {
	    agoo_doc_skip_comment(doc);
	} else {
	    break;
	}
    }
    return (int)(doc->cur - start);
}

int
agoo_doc_skip_jwhite(agooDoc doc) {
    const char	*start = doc->cur;

    for (; 'w' == json_map[*(uint8_t*)doc->cur]; doc->cur++) {
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

// Return true if found, false if not.
bool
agoo_doc_skip_to(agooDoc doc, char c) {
    const char	*orig = doc->cur;

    for (; doc->cur < doc->end; doc->cur++) {
	if (c == *doc->cur) {
	    return true;
	}
    }
    doc->cur = orig;

    return false;
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

void
agoo_doc_read_value_token(agooDoc doc) {
    for (; 't' == value_map[*(uint8_t*)doc->cur]; doc->cur++) {
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

// string delimited by a single quote
int
agoo_doc_read_quote(agooErr err, agooDoc doc) {
    doc->cur++; // skip first '
    if ('\'' == *doc->cur) { // an empty string
	doc->cur++;
	return AGOO_ERR_OK; // empty string
    }
    for (; doc->cur < doc->end; doc->cur++) {
	if ('\'' == *doc->cur) {
	    doc->cur++;
	    break;
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

static gqlValue
read_number(agooErr err, agooDoc doc) {
    int64_t	i = 0;
    int64_t	num = 0;
    int64_t	div = 1;
    int64_t	di = 0;
    long	exp = 0;
    int		neg = false;
    int		hasExp = false;
    int		dec_cnt = 0;
    int		d;

    if ('-' == *doc->cur) {
	doc->cur++;
	neg = true;
    } else if ('+' == *doc->cur) {
	doc->cur++;
    }
    for (; '0' <= *doc->cur && *doc->cur <= '9'; doc->cur++) {
	if (0 < i) {
	    dec_cnt++;
	}
	d = *doc->cur - '0';
	i = i * 10 + d;
	if (INT64_MAX <= (uint64_t)i) {
	    agoo_doc_err(doc, err, "number is too large");
	    return NULL;
	}
    }
    if ('.' == *doc->cur) {
	doc->cur++;
	for (; '0' <= *doc->cur && *doc->cur <= '9'; doc->cur++) {
	    d = (*doc->cur - '0');
	    if (0 < num || 0 < i) {
		dec_cnt++;
	    }
	    num = num * 10 + d;
	    div *= 10;
	    di++;
	    if (INT64_MAX <= (uint64_t)div || DEC_MAX < dec_cnt) {
		agoo_doc_err(doc, err, "number has too many digits");
		return NULL;
	    }
	}
	if ('e' == *doc->cur || 'E' == *doc->cur) {
	    int	eneg = 0;

	    hasExp = true;
	    doc->cur++;
	    if ('-' == *doc->cur) {
		doc->cur++;
		eneg = true;
	    } else if ('+' == *doc->cur) {
		doc->cur++;
	    }
	    for (; '0' <= *doc->cur && *doc->cur <= '9'; doc->cur++) {
		exp = exp * 10 + (*doc->cur - '0');
		if (EXP_MAX <= exp) {
		    agoo_doc_err(doc, err, "number has too many digits");
		    return NULL;
		}
	    }
	    if (eneg) {
		exp = -exp;
	    }
	}
    }
    if (hasExp || 0 < num) {
	long double	d = (long double)i * (long double)div + (long double)num;
	int		x = (int)((int64_t)exp - di);

	d = roundl(d);
	if (0 < x) {
	    d *= powl(10.0L, x);
	} else if (0 > x) {
	    d /= powl(10.0L, -x);
	}
	if (neg) {
	    d = -d;
	}
	return gql_float_create(err, (double)d);

    }
    if (INT32_MAX < i) {
	if (neg) {
	    return gql_i64_create(err, -i);
	} else {
	    return gql_i64_create(err, i);
	}
    }
    if (neg) {
	return gql_int_create(err, (int32_t)-i);
    }
    return gql_int_create(err, (int32_t)i);
}

gqlValue
agoo_doc_read_value(agooErr err, agooDoc doc, gqlType type) {
    gqlValue	value = NULL;
    const char	*start;

    agoo_doc_skip_white(doc);
    start = doc->cur;
    switch (*doc->cur) {
    case '$':
	doc->cur++;
	start++;
	agoo_doc_read_token(doc);
	value = gql_var_create(err, start, doc->cur - start);
	break;
    case '"': {
	const char	*end;

	start++;
	if (AGOO_ERR_OK != agoo_doc_read_string(err, doc)) {
	    return NULL;
	}
	if ('"' == *start) { // must be a """
	    start += 2;
	    end = doc->cur - 3;
	} else {
	    end = doc->cur - 1;
	}
	value = gql_string_create(err, start, end - start);
	break;
    }
    case '-':
    case '+':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	value = read_number(err, doc);
	break;
    case 'n':
	agoo_doc_read_token(doc);
	if (doc->cur - start == 4 && 0 == strncmp("null", start, 4)) {
	    value = gql_null_create(err);
	} else {
	    value = gql_token_create(err, start, doc->cur - start, type);
	}
	break;
    case 't':
	agoo_doc_read_token(doc);
	if (doc->cur - start == 4 && 0 == strncmp("true", start, 4)) {
	    value = gql_bool_create(err, true);
	} else {
	    value = gql_token_create(err, start, doc->cur - start, type);
	}
	break;
    case 'f':
	agoo_doc_read_token(doc);
	if (doc->cur - start == 5 && 0 == strncmp("false", start, 5)) {
	    value = gql_bool_create(err, false);
	} else {
	    value = gql_token_create(err, start, doc->cur - start, type);
	}
	break;
    case '[':
	if (NULL != type) {
	    if (GQL_LIST != type->kind) {
		agoo_doc_err(doc, err, "Not a list type");
		return NULL;
	    }
	    type = type->base;
	}
	if (NULL== (value = gql_list_create(err, type))) {
	    return NULL;
	}
	doc->cur++;
	while (doc->cur < doc->end) {
	    gqlValue	member;

	    agoo_doc_skip_white(doc);
	    if (']' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    if (NULL == (member = agoo_doc_read_value(err, doc, type))) {
		return NULL;
	    }
	    if (AGOO_ERR_OK != gql_list_append(err, value, member)) {
		return NULL;
	    }
	}
	break;
    case '{':
	if (NULL == (value = gql_object_create(err))) {
	    return NULL;
	}
	//value->type = type; // values are always the base object type
	doc->cur++;
	while (doc->cur < doc->end) {
	    char	key[256];
	    gqlValue	member;

	    agoo_doc_skip_white(doc);
	    if ('}' == *doc->cur) {
		doc->cur++;
		break;
	    }
	    start = doc->cur;
	    agoo_doc_read_token(doc);
	    if (start == doc->cur) {
		agoo_doc_err(doc, err, "Expected a member name.");
		return NULL;
	    }
	    if ((int)sizeof(key) <= doc->cur - start) {
		agoo_doc_err(doc, err, "Member name too long.");
		return NULL;
	    }
	    strncpy(key, start, doc->cur - start);
	    key[doc->cur - start] = '\0';
	    agoo_doc_read_token(doc);
	    if (':' != *doc->cur) {
		agoo_doc_err(doc, err, "Expected a ':' after member name.");
		return NULL;
	    }
	    doc->cur++;
	    if (NULL == (member = agoo_doc_read_value(err, doc, type))) {
		return NULL;
	    }
	    if (AGOO_ERR_OK != gql_object_set(err, value, key, member)) {
		return NULL;
	    }
	}
	break;
    default: // Enum value
	agoo_doc_read_token(doc);
	value = gql_token_create(err, start, doc->cur - start, type);
	break;
    }
    return value;
}
