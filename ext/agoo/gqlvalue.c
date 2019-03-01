// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "gqlvalue.h"
#include "graphql.h"
#include "sectime.h"

static const char	spaces[256] = "\n                                                                                                                                                                                                                                                               ";

// Null type
static agooText
null_to_text(agooText text, gqlValue value, int indent, int depth) {
    return agoo_text_append(text, "null", 4);
}

struct _gqlType	gql_null_type = {
    .name = "Null",
    .desc = "Null scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_NULL,
    .core = true,
    .destroy = NULL,
    .to_json = null_to_text,
    .to_sdl = null_to_text,
};

// Int type
static agooText
int_to_text(agooText text, gqlValue value, int indent, int depth) {
    char	num[32];
    int		cnt;

    cnt = snprintf(num, sizeof(num), "%lld", (long long)value->i);

    return agoo_text_append(text, num, cnt);
}

struct _gqlType	gql_int_type = {
    .name = "Int",
    .desc = "Int scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_INT,
    .core = true,
    .destroy = NULL,
    .to_json = int_to_text,
    .to_sdl = int_to_text,
};

// I64 type, add on type.
static agooText
i64_to_text(agooText text, gqlValue value, int indent, int depth) {
    char	num[32];
    int		cnt;

    cnt = snprintf(num, sizeof(num), "%lld", (long long)value->i64);

    return agoo_text_append(text, num, cnt);
}

struct _gqlType	gql_i64_type = {
    .name = "I64",
    .desc = "64 bit integer scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_I64,
    .core = true,
    .destroy = NULL,
    .to_json = i64_to_text,
    .to_sdl = i64_to_text,
};

// String type
static void
string_destroy(gqlValue value) {
    if (value->str.alloced) {
	AGOO_FREE((char*)value->str.ptr);
    }
}

static agooText
string_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (!value->str.alloced) {
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append_json(text, value->str.a, -1);
	text = agoo_text_append(text, "\"", 1);
    } else if (NULL == value->str.ptr) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append_json(text, value->str.ptr, -1);
	text = agoo_text_append(text, "\"", 1);
    }
    return text;
}

struct _gqlType	gql_string_type = {
    .name = "String",
    .desc = "String scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_STRING,
    .core = true,
    .destroy = string_destroy,
    .to_json = string_to_text,
    .to_sdl = string_to_text,
};

// Token type
static agooText
token_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (!value->str.alloced) {
	text = agoo_text_append_json(text, value->str.a, -1);
    } else if (NULL == value->str.ptr) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = agoo_text_append_json(text, value->str.ptr, -1);
    }
    return text;
}

struct _gqlType	gql_token_type = {
    .name = "Token",
    .desc = "Token scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_TOKEN,
    .core = true,
    .destroy = string_destroy,
    .to_json = string_to_text,
    .to_sdl = token_to_text,
};

// ID type
struct _gqlType	gql_id_type = {
    .name = "ID",
    .desc = "ID scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_ID,
    .core = true,
    .destroy = string_destroy,
    .to_json = string_to_text,
    .to_sdl = string_to_text,
};

// Variable type ($var)
static agooText
var_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (!value->str.alloced) {
	text = agoo_text_append_char(text, '%');
	text = agoo_text_append_json(text, value->str.a, -1);
    } else if (NULL == value->str.ptr) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = agoo_text_append_char(text, '%');
	text = agoo_text_append_json(text, value->str.ptr, -1);
    }
    return text;
}

struct _gqlType	gql_var_type = {
    .name = "Var",
    .desc = "Variable scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_VAR,
    .core = true,
    .destroy = string_destroy,
    .to_json = var_to_text,
    .to_sdl = var_to_text,
};

// Bool type
static agooText
bool_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (value->b) {
	text = agoo_text_append(text, "true", 4);
    } else {
	text = agoo_text_append(text, "false", 5);
    }
    return text;
}

struct _gqlType	gql_bool_type = {
    .name = "Boolean",
    .desc = "Boolean scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_BOOL,
    .core = true,
    .destroy = NULL,
    .to_json = bool_to_text,
    .to_sdl = bool_to_text,
};

// Float type
static agooText
float_to_text(agooText text, gqlValue value, int indent, int depth) {
    char	num[32];
    int		cnt;

    cnt = snprintf(num, sizeof(num), "%g", value->f);

    return agoo_text_append(text, num, cnt);
}

struct _gqlType	gql_float_type = {
    .name = "Float",
    .desc = "Float scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_FLOAT,
    .core = true,
    .destroy = NULL,
    .to_json = float_to_text,
    .to_sdl = float_to_text,
};

// Time type
static const char*
read_num(const char *s, int len, int *vp) {
    uint32_t	v = 0;

    for (; 0 < len; len--, s++) {
	if ('0' <= *s && *s <= '9') {
	    v = v * 10 + *s - '0';
	} else {
	    return NULL;
	}
    }
    *vp = (int)v;

    return s;
}

static const char*
read_zone(const char *s, int *vp) {
    int	hr;
    int	min;

    if (NULL == (s = read_num(s, 2, &hr))) {
	return NULL;
    }
    if (':' != *s) {
	return NULL;
    }
    s++;
    if (NULL == (s = read_num(s, 2, &min))) {
	return NULL;
    }
    *vp = hr * 60 + min;

    return s;
}

static int64_t
time_parse(agooErr err, const char *str, int len) {
    const char	*s = str;
    const char	*end;
    struct tm	tm;
    bool	neg = false;
    uint64_t	nsecs = 0;
    int		i = 9;
    int64_t	secs;

    if (0 > len) {
	len = (int)strlen(str);
    }
    if (len < 10 || 36 < len) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    end = str + len;
    memset(&tm, 0, sizeof(tm));
    if ('-' == *s) {
	s++;
	neg = true;
    }
    if (NULL == (s = read_num(s, 4, &tm.tm_year))) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    if (neg) {
	tm.tm_year = -tm.tm_year;
	neg = false;
    }
    tm.tm_year -= 1900;
    if ('-' != *s) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    s++;

    if (NULL == (s = read_num(s, 2, &tm.tm_mon))) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    tm.tm_mon--;
    if ('-' != *s) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    s++;

    if (NULL == (s = read_num(s, 2, &tm.tm_mday))) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }

    // If that's the end then pass back the date as a unix time.
    if (end == s) {
	return (int64_t)timegm(&tm) * 1000000000LL;
    }
    switch (*s++) {
    case 'Z':
	if (s == end) {
	    return (int64_t)timegm(&tm) * 1000000000LL;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    case '-':
	neg = true;
	// fall through
    case '+': {
	int	v = 0;

	if (NULL == (s = read_zone(s, &v))) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if (neg) {
	    v = -v;
	}
	if (s == end) {
	    return ((int64_t)timegm(&tm) - (int64_t)v) * 1000000000LL;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    case 'T':
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    // T encountered, need space for time and zone
    if (end - s < 9) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    if (NULL == (s = read_num(s, 2, &tm.tm_hour))) {
	return 0;
    }
    if (':' != *s) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    s++;

    if (NULL == (s = read_num(s, 2, &tm.tm_min))) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    if (':' != *s) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    s++;

    if (NULL == (s = read_num(s, 2, &tm.tm_sec))) {
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    switch (*s++) {
    case 'Z':
	if (s == end) {
	    return (int64_t)timegm(&tm) * 1000000000LL;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    case '-':
	neg = true;
	// fall through
    case '+': {
	int	v = 0;

	if (end - s < 5) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if (NULL == (s = read_zone(s, &v))) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if (neg) {
	    v = -v;
	}
	if (s == end) {
	    return ((int64_t)timegm(&tm) - (int64_t)v) * 1000000000LL;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    case '.':
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    for (; 0 < i; i--, s++) {
	if (end <= s) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if ('0' <= *s && *s <= '9') {
	    nsecs = nsecs * 10 + *s - '0';
	} else {
	    break;
	}
    }
    for (; 0 < i; i--) {
	nsecs *= 10;
    }
    switch (*s++) {
    case 'Z':
	if (s == end) {
	    break;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    case '-':
	neg = true;
	// fall through
    case '+': {
	int	v = 0;

	if (end - s < 5) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if (NULL == (s = read_zone(s, &v))) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
	if (neg) {
	    v = -v;
	}
	if (s == end) {
	    return ((int64_t)timegm(&tm) - (int64_t)v) * 1000000000LL + nsecs;
	}
	agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	return 0;
    }
    default:
	if (s != end) {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Invalid time format.");
	    return 0;
	}
    }
    if (0 <= (secs = (int64_t)timegm(&tm) * 1000000000LL)) {
	return secs + nsecs;
    }
    return secs - nsecs;
}

static agooText
time_to_text(agooText text, gqlValue value, int indent, int depth) {
    char		str[64];
    int			cnt;
    struct _agooTime	at;
    int64_t		tt = value->time;
    time_t		t = (time_t)(tt / 1000000000LL);
    long		nsecs = tt - (int64_t)t * 1000000000LL;

    if (0 > nsecs) {
	nsecs = -nsecs;
    }
    agoo_sectime(t, &at);
    cnt = sprintf(str, "\"%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ\"", at.year, at.mon, at.day, at.hour, at.min, at.sec, (long)nsecs);

    return agoo_text_append(text, str, cnt);
}

struct _gqlType	gql_time_type = {
    .name = "Time",
    .desc = "Time zulu scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_TIME,
    .core = true,
    .destroy = NULL,
    .to_json = time_to_text,
    .to_sdl = time_to_text,
};

// Uuid type
static int
hexVal(int c) {
    int	h = -1;

    if ('0' <= c && c <= '9') {
	h = c - '0';
    } else if ('a' <= c && c <= 'f') {
	h = c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
	h = c - 'A' + 10;
    }
    return h;
}

// 123e4567-e89b-12d3-a456-426655440000
static int
parse_uuid(agooErr err, const char *str, int len, uint64_t *hip, uint64_t *lop) {
    uint64_t	hi = 0;
    uint64_t	lo = 0;
    int		i;
    int		n;

    if (0 >= len) {
	len = (int)strlen(str);
    }
    if (36 != len || '-' != str[8] || '-' != str[13] || '-' != str[18] || '-' != str[23]) {
	return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
    }
    for (i = 0; i < 8; i++, str++) {
	if (0 > (n = hexVal(*str))) {
	    return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
	}
	hi = (hi << 4) + n;
    }
    str++;
    for (i = 0; i < 4; i++, str++) {
	if (0 > (n = hexVal(*str))) {
	    return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
	}
	hi = (hi << 4) + n;
    }
    str++;
    for (i = 0; i < 4; i++, str++) {
	if (0 > (n = hexVal(*str))) {
	    return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
	}
	hi = (hi << 4) + n;
    }
    str++;
    for (i = 0; i < 4; i++, str++) {
	if (0 > (n = hexVal(*str))) {
	    return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
	}
	lo = (lo << 4) + n;
    }
    str++;
    for (i = 0; i < 12; i++, str++) {
	if (0 > (n = hexVal(*str))) {
	    return agoo_err_set(err, AGOO_ERR_PARSE, "not UUID format");
	}
	lo = (lo << 4) + n;
    }
    *hip = hi;
    *lop = lo;

    return AGOO_ERR_OK;
}

static agooText
uuid_to_text(agooText text, gqlValue value, int indent, int depth) {
    char	str[64];
    int		cnt = sprintf(str, "\"%08lx-%04lx-%04lx-%04lx-%012lx\"",
			      (unsigned long)(value->uuid.hi >> 32),
			      (unsigned long)((value->uuid.hi >> 16) & 0x000000000000FFFFUL),
			      (unsigned long)(value->uuid.hi & 0x000000000000FFFFUL),
			      (unsigned long)(value->uuid.lo >> 48),
			      (unsigned long)(value->uuid.lo & 0x0000FFFFFFFFFFFFUL));

    return agoo_text_append(text, str, cnt);
}

struct _gqlType	gql_uuid_type = {
    .name = "Uuid",
    .desc = "UUID scalar.",
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_UUID,
    .core = true,
    .destroy = NULL,
    .to_json = uuid_to_text,
    .to_sdl = uuid_to_text,
};

// List, not visible but used for list values.
static void
list_destroy(gqlValue value) {
    gqlLink	link;

    while (NULL != (link = value->members)) {
	value->members = link->next;
	gql_value_destroy(link->value);
	AGOO_FREE(link);
    }
}

static agooText
list_to_json(agooText text, gqlValue value, int indent, int depth) {
    int		i = indent * depth;
    int		i2 = i + indent;
    int		d2 = depth + 1;
    gqlLink	link;

    if (0 < indent) {
	i++; // for \n
	i2++;
    }
    if ((int)sizeof(spaces) <= i) {
	i2 = sizeof(spaces) - 1;
	i = i2 - indent;
    }
    text = agoo_text_append(text, "[", 1);
    for (link = value->members; NULL != link; link = link->next) {
	if (0 < i2) {
	    text = agoo_text_append(text, spaces, i2);
	}
	text = gql_value_json(text, link->value, indent, d2);
	if (NULL != link->next) {
	    text = agoo_text_append(text, ",", 1);
	}
    }
    if (0 < indent) {
	text = agoo_text_append(text, spaces, i);
    }
    text = agoo_text_append(text, "]", 1);

    return text;
}

static agooText
list_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    int		i = indent * depth;
    int		i2 = i + indent;
    int		d2 = depth + 1;
    gqlLink	link;

    if (0 < indent) {
	i++; // for \n
	i2++;
    }
    if ((int)sizeof(spaces) <= i) {
	i2 = sizeof(spaces) - 1;
	i = i2 - indent;
    }
    text = agoo_text_append(text, "[", 1);
    for (link = value->members; NULL != link; link = link->next) {
	if (0 < i2) {
	    text = agoo_text_append(text, spaces, i2);
	}
	text = link->value->type->to_sdl(text, link->value, indent, d2);
	if (NULL != link->next) {
	    text = agoo_text_append(text, ",", 1);
	}
    }
    if (0 < indent) {
	text = agoo_text_append(text, spaces, i);
    }
    text = agoo_text_append(text, "]", 1);

    return text;
}

struct _gqlType	list_type = { // unregistered
    .name = "__List",
    .desc = NULL,
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_LIST,
    .core = true,
    .destroy = list_destroy,
    .to_json = list_to_json,
    .to_sdl = list_to_sdl,
};

// Object, not visible but used for object values.
static void
object_destroy(gqlValue value) {
    gqlLink	link;

    while (NULL != (link = value->members)) {
	value->members = link->next;
	gql_value_destroy(link->value);
	AGOO_FREE(link->key);
	AGOO_FREE(link);
    }
}

agooText
gql_object_to_json(agooText text, gqlValue value, int indent, int depth) {
    int		i = indent * depth;
    int		i2 = i + indent;
    int		d2 = depth + 1;
    gqlLink	link;

    if (0 < indent) {
	i++; // for \n
	i2++;
    }
    if ((int)sizeof(spaces) <= i) {
	i2 = sizeof(spaces) - 1;
	i = i2 - indent;
    }
    text = agoo_text_append(text, "{", 1);
    for (link = value->members; NULL != link; link = link->next) {
	if (0 < i2) {
	    text = agoo_text_append(text, spaces, i2);
	}
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append(text, link->key, -1);
	if (0 < indent) {
	    text = agoo_text_append(text, "\":", 2);
	} else {
	    text = agoo_text_append(text, "\":", 2);
	}
	text = gql_value_json(text, link->value, indent, d2);
	if (NULL != link->next) {
	    text = agoo_text_append(text, ",", 1);
	}
    }
    if (0 < indent) {
	text = agoo_text_append(text, spaces, i);
    }
    text = agoo_text_append(text, "}", 1);

    return text;
}

agooText
gql_object_to_sdl(agooText text, gqlValue value, int indent, int depth) {
    int		i = indent * depth;
    int		i2 = i + indent;
    int		d2 = depth + 1;
    gqlLink	link;

    if (0 < indent) {
	i++; // for \n
	i2++;
    }
    if ((int)sizeof(spaces) <= i) {
	i2 = sizeof(spaces) - 1;
	i = i2 - indent;
    }
    text = agoo_text_append(text, "{", 1);
    for (link = value->members; NULL != link; link = link->next) {
	if (0 < i2) {
	    text = agoo_text_append(text, spaces, i2);
	}
	text = agoo_text_append(text, link->key, -1);
	if (0 < indent) {
	    text = agoo_text_append(text, ": ", 2);
	} else {
	    text = agoo_text_append(text, ":", 1);
	}
	text = link->value->type->to_sdl(text, link->value, indent, d2);
	if (NULL != link->next) {
	    text = agoo_text_append(text, ",", 1);
	}
    }
    if (0 < indent) {
	text = agoo_text_append(text, spaces, i);
    }
    text = agoo_text_append(text, "}", 1);

    return text;
}

struct _gqlType	object_type = { // unregistered
    .name = "__Object",
    .desc = NULL,
    .kind = GQL_SCALAR,
    .scalar_kind = GQL_SCALAR_OBJECT,
    .core = true,
    .destroy = object_destroy,
    .to_json = gql_object_to_json,
    .to_sdl = gql_object_to_sdl,
};

////////////////////////////////////////////////////////////////////////////////
void
gql_value_destroy(gqlValue value) {
    if (NULL != value) {
	if (GQL_SCALAR == value->type->kind) {
	    if (NULL != value->type->destroy) {
		value->type->destroy(value);
	    }
	} else if (GQL_ENUM == value->type->kind) {
	    string_destroy(value);
	} else {
	    return;
	}
	AGOO_FREE(value);
    }
}

int
gql_value_init(agooErr err) {
    if (AGOO_ERR_OK != gql_type_set(err, &gql_int_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_i64_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_bool_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_float_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_time_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_uuid_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_id_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_string_type)) {
	return err->code;
    }
    return AGOO_ERR_OK;
}

/// set functions /////////////////////////////////////////////////////////////
void
gql_int_set(gqlValue value, int32_t i) {
    value->type = &gql_int_type;
    value->i = i;
}

void
gql_i64_set(gqlValue value, int64_t i) {
    value->type = &gql_i64_type;
    value->i64 = i;
}

static int
string_set(agooErr err, gqlValue value, const char *str, int len) {
    if (NULL == str) {
	value->str.alloced = true;
	value->str.ptr = NULL;
    } else {
	if (0 >= len) {
	    len = (int)strlen(str);
	}
	if (len < (int)sizeof(value->str.a)) {
	    strncpy(value->str.a, str, len);
	    value->str.alloced = false;
	} else {
	    value->str.alloced = true;
	    if (NULL == (value->str.ptr = AGOO_STRNDUP(str, len))) {
		return AGOO_ERR_MEM(err, "strndup()");
	    }
	}
    }
    return AGOO_ERR_OK;
}

int
gql_string_set(agooErr err, gqlValue value, const char *str, int len) {
    value->type = &gql_string_type;

    return string_set(err, value, str, len);
}

int
gql_token_set(agooErr err, gqlValue value, const char *str, int len) {
    value->type = &gql_token_type;

    return string_set(err, value, str, len);
}

int
gql_id_set(agooErr err, gqlValue value, const char *str, int len) {
    value->type = &gql_id_type;

    return string_set(err, value, str, len);
}

void
gql_bool_set(gqlValue value, bool b) {
    value->type = &gql_bool_type;
    value->b = b;
}

void
gql_float_set(gqlValue value, double f) {
    value->type = &gql_float_type;
    value->f = f;
}

void
gql_time_set(gqlValue value, int64_t t) {
    value->type = &gql_time_type;
    value->time = t;
}

int
gql_time_str_set(agooErr err, gqlValue value, const char *str, int len) {
    value->type = &gql_time_type;
    if (0 >= len) {
	len = (int)strlen(str);
    }
    value->time = time_parse(err, str, len);

    return err->code;
}

void
gql_uuid_set(gqlValue value, uint64_t hi, uint64_t lo) {
    value->type = &gql_uuid_type;
    value->uuid.hi = hi;
    value->uuid.lo = lo;
}

int
gql_uuid_str_set(agooErr err, gqlValue value, const char *str, int len) {
    uint64_t	hi = 0;
    uint64_t	lo = 0;

    if (AGOO_ERR_OK != parse_uuid(err, str, len, &hi, &lo)) {
	return err->code;
    }
    value->type = &gql_uuid_type;
    value->uuid.hi = hi;
    value->uuid.lo = lo;

    return AGOO_ERR_OK;
}

extern void	gql_null_set(gqlValue value);

gqlLink
gql_link_create(agooErr err, const char *key, gqlValue item) {
    gqlLink	link = (gqlLink)AGOO_MALLOC(sizeof(struct _gqlLink));

    if (NULL == link) {
	AGOO_ERR_MEM(err, "GraphQL List Link");
    } else {
	link->next = NULL;
	link->key = NULL;
	if (NULL != key) {
	    if (NULL == (link->key = AGOO_STRDUP(key))) {
		AGOO_ERR_MEM(err, "strdup()");
		return NULL;
	    }
	}
	link->value = item;
    }
    return link;
}

void
gql_link_destroy(gqlLink link) {
    AGOO_FREE(link->key);
    if (NULL != link->value) {
	gql_value_destroy(link->value);
    }
    AGOO_FREE(link);
}

int
gql_list_append(agooErr err, gqlValue list, gqlValue item) {
    gqlLink	link = gql_link_create(err, NULL, item);

    if (NULL != link) {
	if (NULL == list->members) {
	    list->members = link;
	} else {
	    gqlLink	last = list->members;

	    for (; NULL != last->next; last = last->next) {
	    }
	    last->next = link;
	}
    }
    return AGOO_ERR_OK;
}

int
gql_list_prepend(agooErr err, gqlValue list, gqlValue item) {
    gqlLink	link = gql_link_create(err, NULL, item);

    if (NULL != link) {
	link->next = list->members;
	list->members = link;
    }
    return AGOO_ERR_OK;
}

int
gql_object_set(agooErr err, gqlValue obj, const char *key, gqlValue item) {
    gqlLink	link = gql_link_create(err, key, item);

    if (NULL != link) {
	//link->next = obj->members;
	if (NULL == obj->members) {
	    obj->members = link;
	} else {
	    gqlLink	last = obj->members;

	    for (; NULL != last->next; last = last->next) {
	    }
	    last->next = link;
	}
    }
    return AGOO_ERR_OK;
}

/// create functions //////////////////////////////////////////////////////////

static gqlValue
value_create(gqlType type) {
    gqlValue	v = (gqlValue)AGOO_CALLOC(1, sizeof(struct _gqlValue));

    if (NULL != v) {
	v->type = type;
    }
    return v;
}

gqlValue
gql_int_create(agooErr err, int32_t i) {
    gqlValue	v = value_create(&gql_int_type);

    if (NULL != v) {
	v->i = i;
    }
    return v;
}

gqlValue
gql_i64_create(agooErr err, int64_t i) {
    gqlValue	v = value_create(&gql_i64_type);

    if (NULL != v) {
	v->i64 = i;
    }
    return v;
}

gqlValue
gql_string_create(agooErr err, const char *str, int len) {
    gqlValue	v;

    if (0 >= len) {
	len = (int)strlen(str);
    }
    if (NULL != (v = value_create(&gql_string_type))) {
	if ((int)sizeof(v->str.a) <= len) {
	    v->str.alloced = true;
	    if (NULL == (v->str.ptr = AGOO_STRNDUP(str, len))) {
		AGOO_ERR_MEM(err, "strdup()");
		return NULL;
	    }
	} else {
	    v->str.alloced = false;
	    strncpy(v->str.a, str, len);
	    v->str.a[len] = '\0';
	}
    }
    return v;
}

gqlValue
gql_token_create(agooErr err, const char *str, int len, gqlType type) {
    gqlValue	v;

    if (0 >= len) {
	len = (int)strlen(str);
    }
    if (NULL == type || GQL_ENUM != type->kind) {
	type = &gql_token_type;
    }
    if (NULL != (v = value_create(type))) {
	if ((int)sizeof(v->str.a) <= len) {
	    v->str.alloced = true;
	    if (NULL == (v->str.ptr = AGOO_STRNDUP(str, len))) {
		AGOO_ERR_MEM(err, "strdup()");
		return NULL;
	    }
	} else {
	    v->str.alloced = false;
	    strncpy(v->str.a, str, len);
	    v->str.a[len] = '\0';
	}
    }
    return v;
}

gqlValue
gql_id_create(agooErr err, const char *str, int len) {
    gqlValue	v;

    if (0 >= len) {
	len = (int)strlen(str);
    }
    if (NULL != (v = value_create(&gql_id_type))) {
	if ((int)sizeof(v->str.a) <= len) {
	    v->str.alloced = true;
	    if (NULL == (v->str.ptr = AGOO_STRNDUP(str, len))) {
		AGOO_ERR_MEM(err, "strdup()");
		return NULL;
	    }
	} else {
	    v->str.alloced = false;
	    strncpy(v->str.a, str, len);
	    v->str.a[len] = '\0';
	}
    }
    return v;
}

gqlValue
gql_var_create(agooErr err, const char *str, int len) {
    gqlValue	v;

    if (0 >= len) {
	len = (int)strlen(str);
    }
    if (NULL != (v = value_create(&gql_var_type))) {
	if ((int)sizeof(v->str.a) <= len) {
	    v->str.alloced = true;
	    if (NULL == (v->str.ptr = AGOO_STRNDUP(str, len))) {
		AGOO_ERR_MEM(err, "strdup()");
		return NULL;
	    }
	} else {
	    v->str.alloced = false;
	    strncpy(v->str.a, str, len);
	    v->str.a[len] = '\0';
	}
    }
    return v;
}

gqlValue
gql_bool_create(agooErr err, bool b) {
    gqlValue	v = value_create(&gql_bool_type);

    if (NULL != v) {
	v->b = b;
    }
    return v;
}

gqlValue
gql_float_create(agooErr err, double f) {
    gqlValue	v = value_create(&gql_float_type);

    if (NULL != v) {
	v->f = f;
    }
    return v;
}

gqlValue
gql_time_create(agooErr err, int64_t t) {
    gqlValue	v = value_create(&gql_time_type);

    if (NULL != v) {
	v->time = t;
    }
    return v;
}

gqlValue
gql_time_str_create(agooErr err, const char *str, int len) {
    gqlValue	v = value_create(&gql_time_type);

    if (NULL != v) {
	if (0 >= len) {
	    len = (int)strlen(str);
	}
	v->time = time_parse(err, str, len);
    }
    return v;
}

gqlValue
gql_uuid_create(agooErr err, uint64_t hi, uint64_t lo) {
    gqlValue	v = value_create(&gql_uuid_type);

    if (NULL != v) {
	v->uuid.hi = hi;
	v->uuid.lo = lo;
    }
    return v;
}

// 123e4567-e89b-12d3-a456-426655440000
gqlValue
gql_uuid_str_create(agooErr err, const char *str, int len) {
    uint64_t	hi = 0;
    uint64_t	lo = 0;
    gqlValue	v;

    if (AGOO_ERR_OK != parse_uuid(err, str, len, &hi, &lo)) {
	return NULL;
    }
    if (NULL != (v = value_create(&gql_uuid_type))) {
	v->uuid.hi = hi;
	v->uuid.lo = lo;
    }
    return v;
}

gqlValue
gql_null_create(agooErr err) {
    gqlValue	v = value_create(&gql_null_type);

    return v;
}

gqlValue
gql_list_create(agooErr err, gqlType item_type) {
    gqlValue	v = value_create(&list_type);

    if (NULL != v) {
	v->members = NULL;
	v->member_type = item_type;
    }
    return v;
}

gqlValue
gql_object_create(agooErr err) {
    gqlValue	v = value_create(&object_type);

    if (NULL != v) {
	v->members = NULL;
	v->member_type = NULL;
    }
    return v;
}

agooText
gql_value_json(agooText text, gqlValue value, int indent, int depth) {
    if (NULL == value->type || GQL_SCALAR != value->type->kind) {
	if (GQL_ENUM == value->type->kind) {
	    text = string_to_text(text, value, indent, depth);
	} else {
	    text = agoo_text_append(text, "null", 4);
	}
    } else if (NULL == value->type->to_json) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = value->type->to_json(text, value, indent, depth);
    }
    return text;
}

agooText
gql_value_sdl(agooText text, gqlValue value, int indent, int depth) {
    if (NULL == value->type || GQL_SCALAR != value->type->kind) {
	if (GQL_ENUM == value->type->kind) {
	    text = token_to_text(text, value, indent, depth);
	} else {
	    text = agoo_text_append(text, "null", 4);
	}
    } else if (NULL == value->type->to_sdl) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = value->type->to_sdl(text, value, indent, depth);
    }
    return text;
}

const char*
gql_string_get(gqlValue value) {
    const char	*s = NULL;

    if (NULL != value) {
	if (&gql_string_type == value->type || &gql_token_type == value->type || &gql_var_type == value->type || &gql_id_type == value->type ||
	    GQL_ENUM == value->type->kind) {
	    if (value->str.alloced) {
		s = value->str.ptr;
	    } else {
		s = value->str.a;
	    }
	}
    }
    return s;
}

static int
convert_to_bool(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_INT:
	gql_bool_set(value, 0 != value->i);
	break;
    case GQL_SCALAR_I64:
	gql_bool_set(value, 0 != value->i64);
	break;
    case GQL_SCALAR_FLOAT:
	gql_bool_set(value, 0.0 != value->f);
	break;
    case GQL_SCALAR_STRING:
    case GQL_SCALAR_TOKEN: {
	const char	*s = gql_string_get(value);

	value->type = &gql_bool_type;
	if (0 == strcasecmp("true", s)) {
	    value->b = true;
	} else if (0 == strcasecmp("false", s)) {
	    value->b = false;
	} else {
	    agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a String of '%s' into a Boolean value.", s);
	}
	if (value->str.alloced) {
	    AGOO_FREE((char*)value->str.ptr);
	}
	break;
    }
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a Boolean value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_int(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_I64:
	if (value->i64 < INT32_MIN || INT32_MAX < value->i64) {
	    agoo_err_set(err, ERANGE, "Can not coerce a %lld into an Int value. Out of range.", (long long)value->i64);
	} else {
	    gql_int_set(value, (int32_t)value->i64);
	}
	break;
    case GQL_SCALAR_FLOAT:
	if (value->f < (double)INT32_MIN || (double)INT32_MAX < value->f) {
	    agoo_err_set(err, ERANGE, "Can not coerce a %g into an Int value. Out of range.", value->f);
	} else if ((double)(int32_t)value->f != value->f) {
	    agoo_err_set(err, ERANGE, "Can not coerce a %g into an Int value. Loss of precision.", value->f);
	} else {
	    gql_int_set(value, (int32_t)value->f);
	}
	break;
    case GQL_SCALAR_STRING: {
	const char	*s = gql_string_get(value);
	char		*end;
	long		i = strtol(s, &end, 10);

	if ('\0' != *end || (0 == i && 0 != errno)) {
	    agoo_err_set(err, ERANGE, "Can not coerce a '%s' into an Int value.", s);
	} else if (i < INT32_MIN || INT32_MAX < i) {
	    agoo_err_set(err, ERANGE, "Can not coerce a %lld into an Int value. Out of range.", (long long)i);
	} else {
	    gql_int_set(value, (int32_t)i);
	    if (value->str.alloced) {
		AGOO_FREE((char*)value->str.ptr);
	    }
	}
	break;
    }
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into an Int value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_i64(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_INT:
	gql_i64_set(value, (int64_t)value->i);
	break;
    case GQL_SCALAR_FLOAT:
	if ((double)(int64_t)value->f != value->f) {
	    agoo_err_set(err, ERANGE, "Can not coerce a %g into an I64 value. Loss of precision.", value->f);
	} else {
	    gql_i64_set(value, (int64_t)value->f);
	}
	break;
    case GQL_SCALAR_STRING: {
	const char	*s = gql_string_get(value);
	char		*end;
	long long	i = strtoll(s, &end, 10);

	if ('\0' != *end || (0 == i && 0 != errno)) {
	    agoo_err_set(err, ERANGE, "Can not coerce a '%s' into an I64 value.", s);
	} else {
	    gql_i64_set(value, (int64_t)i);
	    if (value->str.alloced) {
		AGOO_FREE((char*)value->str.ptr);
	    }
	}
	break;
    }
    case GQL_SCALAR_TIME:
	gql_i64_set(value, (int64_t)value->time);
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into an I64 value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_float(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_INT:
	gql_float_set(value, (double)value->i);
	break;
    case GQL_SCALAR_I64:
	gql_float_set(value, (double)value->i64);
	break;
    case GQL_SCALAR_STRING: {
	const char	*s = gql_string_get(value);
	char		*end;
	double		d = strtod(s, &end);

	if ('\0' != *end) {
	    agoo_err_set(err, ERANGE, "Can not coerce a '%s' into a Float value.", s);
	} else {
	    gql_float_set(value, d);
	    if (value->str.alloced) {
		AGOO_FREE((char*)value->str.ptr);
	    }
	}
	break;
    }
    case GQL_SCALAR_TIME:
	gql_float_set(value, (double)value->time / 1000000000.0);
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a Float value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_string(agooErr err, gqlValue value) {
    char	buf[64];
    int		cnt;

    switch (value->type->scalar_kind) {
    case GQL_SCALAR_BOOL:
	if (value->b) {
	    gql_string_set(err, value, "true", 4);
	} else {
	    gql_string_set(err, value, "false", 5);
	}
	break;
    case GQL_SCALAR_INT:
	cnt = sprintf(buf, "%d", value->i);
	gql_string_set(err, value, buf, cnt);
	break;
    case GQL_SCALAR_I64:
	cnt = sprintf(buf, "%lld", (long long)value->i64);
	gql_string_set(err, value, buf, cnt);
	break;
    case GQL_SCALAR_FLOAT:
	cnt = sprintf(buf, "%g", value->f);
	gql_string_set(err, value, buf, cnt);
	break;
    case GQL_SCALAR_TOKEN:
	value->type = &gql_string_type;
	break;
    case GQL_SCALAR_ID:
	value->type = &gql_string_type;
	break;
    case GQL_SCALAR_TIME: {
	struct _agooTime	at;
	int64_t			tt = value->time;
	time_t			t = (time_t)(tt / 1000000000LL);
	long			nsecs = tt - (int64_t)t * 1000000000LL;

	if (0 > nsecs) {
	    nsecs = -nsecs;
	}
	agoo_sectime(t, &at);
	cnt = sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ", at.year, at.mon, at.day, at.hour, at.min, at.sec, (long)nsecs);
	gql_string_set(err, value, buf, cnt);
	break;
    }
    case GQL_SCALAR_UUID:
	cnt = sprintf(buf, "%08lx-%04lx-%04lx-%04lx-%012lx",
		      (unsigned long)(value->uuid.hi >> 32),
		      (unsigned long)((value->uuid.hi >> 16) & 0x000000000000FFFFUL),
		      (unsigned long)(value->uuid.hi & 0x000000000000FFFFUL),
		      (unsigned long)(value->uuid.lo >> 48),
		      (unsigned long)(value->uuid.lo & 0x0000FFFFFFFFFFFFUL));
	gql_string_set(err, value, buf, cnt);
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a String value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_token(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_STRING:
    case GQL_SCALAR_ID:
	value->type = &gql_token_type;
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a token value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_id(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_STRING:
    case GQL_SCALAR_TOKEN:
	value->type = &gql_id_type;
	break;
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a ID value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_time(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_I64:
	gql_time_set(value, value->i64);
	break;
    case GQL_SCALAR_STRING: {
	int64_t	nsecs = time_parse(err, gql_string_get(value), -1);

	if (AGOO_ERR_OK == err->code) {
	    if (value->str.alloced) {
		AGOO_FREE((char*)value->str.ptr);
	    }
	    gql_time_set(value, nsecs);
	}
	break;
    }
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a Time value.", value->type->name);
	break;
    }
    return err->code;
}

static int
convert_to_uuid(agooErr err, gqlValue value) {
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_STRING: {
	const char	*s = gql_string_get(value);
	bool		alloced = value->str.alloced;

	if (AGOO_ERR_OK == gql_uuid_str_set(err, value, s, 0)) {
	    if (alloced) {
		AGOO_FREE((char*)s);
	    }
	}
	break;
    }
    default:
	agoo_err_set(err, AGOO_ERR_PARSE, "Can not coerce a %s into a UUID value.", value->type->name);
	break;
    }
    return err->code;
}

int
gql_value_convert(agooErr err, gqlValue value, gqlType type) {
    int	code = AGOO_ERR_OK;

    if (type != value->type) {
	switch (type->scalar_kind) {
	case GQL_SCALAR_BOOL:
	    code = convert_to_bool(err, value);
	    break;
	case GQL_SCALAR_INT:
	    code = convert_to_int(err, value);
	    break;
	case GQL_SCALAR_I64:
	    code = convert_to_i64(err, value);
	    break;
	case GQL_SCALAR_FLOAT:
	    code = convert_to_float(err, value);
	    break;
	case GQL_SCALAR_STRING:
	    code = convert_to_string(err, value);
	    break;
	case GQL_SCALAR_TOKEN:
	    code = convert_to_token(err, value);
	    break;
	case GQL_SCALAR_ID:
	    code = convert_to_id(err, value);
	    break;
	case GQL_SCALAR_TIME:
	    code = convert_to_time(err, value);
	    break;
	case GQL_SCALAR_UUID:
	    code = convert_to_uuid(err, value);
	    break;
	default:
	    break;
	}
    }
    return code;
}

gqlValue
gql_value_dup(agooErr err, gqlValue value) {
    gqlValue	dup = value_create(value->type);

    if (NULL == dup) {
	AGOO_ERR_MEM(err, "GraphQL Value");
	return NULL;
    }
    switch (value->type->scalar_kind) {
    case GQL_SCALAR_BOOL:
	dup->b = value->b;
	break;
    case GQL_SCALAR_INT:
	dup->i = value->i;
	break;
    case GQL_SCALAR_I64:
	dup->i64 = value->i64;
	break;
    case GQL_SCALAR_FLOAT:
	dup->f = value->f;
	break;
    case GQL_SCALAR_STRING:
    case GQL_SCALAR_TOKEN:
    case GQL_SCALAR_ID:
	if (value->str.alloced) {
	    if (NULL == (dup->str.ptr = strdup(value->str.ptr))) {
		AGOO_ERR_MEM(err, "strdup()");
		AGOO_FREE(dup);
		dup = NULL;
	    }
	} else {
	    memcpy(dup->str.a, value->str.a, sizeof(value->str.a));
	}
	break;
    case GQL_SCALAR_TIME:
	dup->time = value->time;
	break;
    case GQL_SCALAR_UUID:
	dup->uuid.hi = value->uuid.hi;
	dup->uuid.lo = value->uuid.lo;
	break;
    default:
	break;
    }
    return dup;
}
