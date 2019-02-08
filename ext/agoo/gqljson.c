// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doc.h"
#include "err.h"
#include "gqljson.h"
#include "gqlvalue.h"
#include "graphql.h"

#define EXP_MAX		1023
#define DEC_MAX		14
#define DIV_MAX		100000000000000000LL
#define BIGGISH		922337203685477580LL
#ifndef	INT32_MAX
#define INT32_MAX       2147483647LL
#endif
#ifndef	INT64_MAX
#define INT64_MAX       9223372036854775807LL
#endif

static gqlValue	parse_value(agooErr err, agooDoc doc);

static gqlValue
parse_num(agooErr err, agooDoc doc) {
    gqlValue	value = NULL;
    int		zero_cnt = 0;
    int64_t	i = 0;
    int64_t	num = 0;
    int64_t	div = 1;
    long	exp = 0;
    int		dec_cnt = 0;
    bool	big = false;
    bool	neg = false;
    char	c;
    const char	*start = doc->cur;

    c = *doc->cur;
    if ('-' == c) {
	doc->cur++;
	c = *doc->cur;
	neg = true;
    } else if ('+' == c) {
	doc->cur++;
	c = *doc->cur;
    }
    for (; doc->cur < doc->end && '0' <= c && c <= '9'; c = *++doc->cur) {
	dec_cnt++;
	if (!big) {
	    int	d = (c - '0');

	    if (0 == d) {
		zero_cnt++;
	    } else {
		zero_cnt = 0;
	    }
	    if (BIGGISH <= i && (INT64_MAX - i * 10LL) < (int64_t)d) {
		big = true;
	    } else {
		i = i * 10 + d;
	    }
	}
    }
    if ('.' == c) {
	c = *++doc->cur;
	for (; doc->cur < doc->end && '0' <= c && c <= '9'; c = *++doc->cur) {
	    int	d = (c - '0');

	    if (0 == d) {
		zero_cnt++;
	    } else {
		zero_cnt = 0;
	    }
	    dec_cnt++;
	    if ((BIGGISH < num && (INT64_MAX - (int64_t)d) / 10LL <= num) ||
		DIV_MAX <= div || DEC_MAX < dec_cnt - zero_cnt) {
		big = true;
	    } else {
		num = num * 10 + d;
		div *= 10;
	    }
	}
    }
    if ('e' == c || 'E' == c) {
	int	eneg = 0;

	c = *++doc->cur;
	if ('-' == c) {
	    c = *++doc->cur;
	    eneg = 1;
	} else if ('+' == c) {
	    c = *++doc->cur;
	}
	for (; doc->cur < doc->end && '0' <= c && c <= '9'; c = *++doc->cur) {
	    exp = exp * 10 + (c - '0');
	    if (EXP_MAX <= exp) {
		big = true;
	    }
	}
	if (eneg) {
	    exp = -exp;
	}
    }
    dec_cnt -= zero_cnt;
    if (big) {
	value = gql_string_create(err, start, (int)(doc->cur - start));
    } else if (1 == div && 0 == exp) {
	int64_t	si = i;
	if (neg) {
	    si = -si;
	}
	if (INT32_MAX < i) {
	    value = gql_i64_create(err, si);
	} else {
	    value = gql_int_create(err, si);
	}
    } else { // decimal
	double	d = (double)i + (double)num / (double)div;

	if (neg) {
	    d = -d;
	}
	if (0 != exp) {
	    d *= pow(10.0, exp);
	}
	value = gql_float_create(err, d);
    }
    return value;
}

static uint32_t
read_hex(agooErr err, agooDoc doc) {
    uint32_t	hex = 0;
    const char	*end = doc->cur + 4;
    char	c;

    if (doc->end < end) {
	agoo_doc_err(doc, err, "invalid escaped character");
	return 0;
    }
    for (; doc->cur < end; doc->cur++) {
	c = *doc->cur;
	hex = hex << 4;
	if ('0' <= c && c <= '9') {
	    hex += c - '0';
	} else if ('A' <= c && c <= 'F') {
	    hex += c - 'A' + 10;
	} else if ('a' <= c && c <= 'f') {
	    hex += c - 'a' + 10;
	} else {
	    agoo_doc_err(doc, err, "invalid unicode character");
	    return 0;
	}
    }
    return hex;
}

static agooText
unicode_to_chars(agooErr err, agooText t, uint32_t code, agooDoc doc) {
    if (0x0000007F >= code) {
	t = agoo_text_append_char(t, (char)code);
    } else if (0x000007FF >= code) {
	t = agoo_text_append_char(t, 0xC0 | (code >> 6));
	t = agoo_text_append_char(t, 0x80 | (0x3F & code));
    } else if (0x0000FFFF >= code) {
	t = agoo_text_append_char(t, 0xE0 | (code >> 12));
	t = agoo_text_append_char(t, 0x80 | ((code >> 6) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | (0x3F & code));
    } else if (0x001FFFFF >= code) {
	t = agoo_text_append_char(t, 0xF0 | (code >> 18));
	t = agoo_text_append_char(t, 0x80 | ((code >> 12) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 6) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | (0x3F & code));
    } else if (0x03FFFFFF >= code) {
	t = agoo_text_append_char(t, 0xF8 | (code >> 24));
	t = agoo_text_append_char(t, 0x80 | ((code >> 18) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 12) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 6) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | (0x3F & code));
    } else if (0x7FFFFFFF >= code) {
	t = agoo_text_append_char(t, 0xFC | (code >> 30));
	t = agoo_text_append_char(t, 0x80 | ((code >> 24) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 18) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 12) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | ((code >> 6) & 0x3F));
	t = agoo_text_append_char(t, 0x80 | (0x3F & code));
    } else {
	agoo_doc_err(doc, err, "invalid unicode character");
    }
    if (NULL == t) {
	AGOO_ERR_MEM(err, "text");
    }
    return t;
}

static gqlValue
parse_escaped(agooErr err, agooDoc doc) {
    agooText	t = agoo_text_allocate(4096);
    gqlValue	value = NULL;

    if (NULL == t) {
	return NULL;
    }
    for (; doc->cur < doc->end && '"' != *doc->cur; doc->cur++) {
	if ('\\' == *doc->cur) {
	    doc->cur++;
	    switch (*doc->cur) {
	    case 'n':	t = agoo_text_append_char(t, '\n');	break;
	    case 'r':	t = agoo_text_append_char(t, '\r');	break;
	    case 't':	t = agoo_text_append_char(t, '\t');	break;
	    case 'f':	t = agoo_text_append_char(t, '\f');	break;
	    case 'b':	t = agoo_text_append_char(t, '\b');	break;
	    case '"':	t = agoo_text_append_char(t, '\"');	break;
	    case '/':	t = agoo_text_append_char(t, '/');	break;
	    case '\\':	t = agoo_text_append_char(t, '\\');	break;
	    case 'u': {
		uint32_t	code;

		doc->cur++;
		if (0 == (code = read_hex(err, doc)) && AGOO_ERR_OK != err->code) {
		    goto DONE;
		}
		if (0x0000D800 <= code && code <= 0x0000DFFF) {
		    uint32_t	c1 = (code - 0x0000D800) & 0x000003FF;
		    uint32_t	c2;

		    if ('\\' != *doc->cur || 'u' != doc->cur[1]) {
			agoo_doc_err(doc, err, "invalid unicode character");
			goto DONE;
		    }
		    doc->cur += 2;
		    if (0 == (c2 = read_hex(err, doc)) && AGOO_ERR_OK != err->code) {
			goto DONE;
		    }
		    c2 = (c2 - 0x0000DC00) & 0x000003FF;
		    code = ((c1 << 10) | c2) + 0x00010000;
		}
		t = unicode_to_chars(err, t, code, doc);
		if (AGOO_ERR_OK != err->code) {
		    goto DONE;
		}
		doc->cur--;
		break;
	    }
	    default:
		agoo_doc_err(doc, err, "invalid escaped character");
		agoo_text_release(t);
		return NULL;
	    }
	} else if (NULL == (t = agoo_text_append(t, doc->cur, 1))) {
	    AGOO_ERR_MEM(err, "text");
	    return NULL;
	}
    }
    value = gql_string_create(err, t->text, t->len);
    doc->cur++; // past trailing "
DONE:
    agoo_text_release(t);

    return value;
}

static gqlValue
parse_string(agooErr err, agooDoc doc) {
    const char	*start;

    if ('"' != *doc->cur) {
	agoo_doc_err(doc, err, "invalid string");
	return NULL;
    }
    doc->cur++;
    start = doc->cur;
    for (; doc->cur < doc->end && '"' != *doc->cur; doc->cur++) {
	if ('\\' == *doc->cur) {
	    doc->cur = start;
	    return parse_escaped(err, doc);
	}
    }
    doc->cur++;

    return gql_string_create(err, start, (int)(doc->cur - start - 1));
}

static gqlValue
return_parse_err(agooErr err, agooDoc doc, const char *msg, gqlValue value) {
    agoo_doc_err(doc, err, msg);
    gql_value_destroy(value);

    return NULL;
}

static gqlValue
parse_object(agooErr err, agooDoc doc) {
    char	key[256];
    gqlValue	value = gql_object_create(err);
    gqlValue	member;
    const char	*start;

    if (NULL == value) {
	return NULL;
    }
    doc->cur++; // past the {
    agoo_doc_skip_jwhite(doc);
    if ('}' != *doc->cur) {
	for (; doc->cur < doc->end; doc->cur++) {
	    if ('"' != *doc->cur) {
		return return_parse_err(err, doc, "expected an object key as a string", value);
	    }
	    doc->cur++; // past "

	    // TBD support unicode and escaped characters in key

	    start = doc->cur;
	    for (; doc->cur < doc->end && '"' != *doc->cur; doc->cur++) {
	    }
	    if ((int)sizeof(key) <= (int)(doc->cur - start)) {
		return return_parse_err(err, doc, "object key too long", value);
	    }
	    memcpy(key, start, doc->cur - start);
	    key[doc->cur - start] = '\0';
	    doc->cur++; // past "
	    agoo_doc_skip_jwhite(doc);
	    if (':' != *doc->cur) {
		return return_parse_err(err, doc, "expected a colon", value);
	    }
	    doc->cur++;
	    agoo_doc_skip_jwhite(doc);
	    if (NULL == (member = parse_value(err, doc)) ||
		AGOO_ERR_OK != gql_object_set(err, value, key, member)) {
		gql_value_destroy(value);
		return NULL;
	    }
	    agoo_doc_skip_jwhite(doc);
	    if ('}' == *doc->cur) {
		break;
	    }
	    if (',' != *doc->cur) {
		return return_parse_err(err, doc, "expected a comma", value);
	    }
	}
    }
    if ('}' != *doc->cur) {
	agoo_doc_err(doc, err, "object not terminated");
	gql_value_destroy(value);
	return NULL;
    }
    doc->cur++;

    return value;
}

static gqlValue
parse_array(agooErr err, agooDoc doc) {
    gqlValue	value = gql_list_create(err, NULL);
    gqlValue	member;

    if (NULL == value) {
	return NULL;
    }
    doc->cur++; // past the [
    agoo_doc_skip_jwhite(doc);
    if (']' != *doc->cur) {
	for (; doc->cur < doc->end; doc->cur++) {
	    if (NULL == (member = parse_value(err, doc)) ||
		AGOO_ERR_OK != gql_list_append(err, value, member)) {
		gql_value_destroy(value);
		return NULL;
	    }
	    agoo_doc_skip_jwhite(doc);
	    if (']' == *doc->cur) {
		break;
	    }
	    if (',' != *doc->cur) {
		agoo_doc_err(doc, err, "expected a comma");
		gql_value_destroy(value);
		return NULL;
	    }
	}
    }
    if (']' != *doc->cur) {
	agoo_doc_err(doc, err, "array not terminated");
	gql_value_destroy(value);
	return NULL;
    }
    doc->cur++;

    return value;
}

static gqlValue
parse_value(agooErr err, agooDoc doc) {
    gqlValue	value = NULL;

    agoo_doc_skip_jwhite(doc);
    switch (*doc->cur) {
    case '{':
	value = parse_object(err, doc);
	break;
    case '[':
	value = parse_array(err, doc);
	break;
    case '"':
	value = parse_string(err, doc);
	break;
    case '+':
    case '-':
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
	value = parse_num(err, doc);
	break;
    case 't':
	if (4 <= doc->end - doc->cur &&
	    'r' == doc->cur[1] && 'u' == doc->cur[2] && 'e' == doc->cur[3]) {
	    value = gql_bool_create(err, true);
	} else {
	    agoo_doc_err(doc, err, "invalid token");
	}
	break;
    case 'f':
	if (5 <= doc->end - doc->cur &&
	    'a' == doc->cur[1] && 'l' == doc->cur[2] && 's' == doc->cur[3] && 'e' == doc->cur[4]) {
	    value = gql_bool_create(err, false);
	} else {
	    agoo_doc_err(doc, err, "invalid token");
	}
	break;
    case 'n':
	if (4 <= doc->end - doc->cur &&
	    'u' == doc->cur[1] && 'l' == doc->cur[2] && 'l' == doc->cur[3]) {
	    value = gql_null_create(err);
	} else {
	    agoo_doc_err(doc, err, "invalid token");
	}
	break;
    case '\0':
	agoo_doc_err(doc, err, "embedded null character");
	break;
    default:
	agoo_doc_err(doc, err, "invalid token character");
	break;
    }
    return value;
}

gqlValue
gql_json_parse(agooErr err, const char *json, size_t len) {
    struct _agooDoc	doc;

    agoo_doc_init(&doc, json, len);

    return parse_value(err, &doc);
}
