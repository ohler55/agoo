// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "gqlvalue.h"
#include "graphql.h"

static const char	spaces[256] = "\n                                                                                                                                                                                                                                                               ";

// Int type
static agooText
null_to_text(agooText text, gqlValue value, int indent, int depth) {
    return agoo_text_append(text, "null", 4);
}

struct _gqlType	gql_null_type = {
    .name = "Null",
    .desc = "Null scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = null_to_text,
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = int_to_text,
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = i64_to_text,
};

// String type
static void
string_destroy(gqlValue value) {
    free((char*)value->str);
}

static agooText	string_to_text(agooText text, gqlValue value, int indent, int depth);

struct _gqlType	gql_string_type = {
    .name = "String",
    .desc = "String scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = string_destroy,
    .to_json = string_to_text,
};

// Alternative to String but with no destroy needed.
struct _gqlType	gql_str16_type = { // unregistered
    .name = "str16",
    .desc = NULL,
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = string_to_text,
};

static agooText
string_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (&gql_str16_type == value->type) {
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append(text, value->str16, -1);
	text = agoo_text_append(text, "\"", 1);
    } else if (NULL == value->str) {
	text = agoo_text_append(text, "null", 4);
    } else {
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append(text, value->str, -1);
	text = agoo_text_append(text, "\"", 1);
    }
    return text;
}

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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = bool_to_text,
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = float_to_text,
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
    char	str[64];
    int		cnt;
    struct tm	tm;
    int64_t	tt = value->time;
    time_t	t = (time_t)(tt / 1000000000LL);
    long	nsecs = tt - (int64_t)t * 1000000000LL;

    if (0 > nsecs) {
	nsecs = -nsecs;
    }
    gmtime_r(&t, &tm);
    cnt = sprintf(str, "\"%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ\"",
		  1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec, (long)nsecs);
    
    return agoo_text_append(text, str, cnt);
}

struct _gqlType	gql_time_type = {
    .name = "Time",
    .desc = "Time zulu scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = time_to_text,
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = NULL,
    .to_json = uuid_to_text,
};

// Url type
static void
url_destroy(gqlValue value) {
    free((char*)value->url);
}

static agooText
url_to_text(agooText text, gqlValue value, int indent, int depth) {
    if (NULL == value->url) {
	return agoo_text_append(text, "null", 4);
    }
    text = agoo_text_append(text, "\"", 1);
    text = agoo_text_append(text, value->url, -1);
    text = agoo_text_append(text, "\"", 1);

    return text;
}

struct _gqlType	gql_url_type = {
    .name = "Url",
    .desc = "URL scalar.",
    .kind = GQL_SCALAR,
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = url_destroy,
    .to_json = url_to_text,
};

// List, not visible but used for list values.
static void
list_destroy(gqlValue value) {
    gqlLink	link;

    while (NULL != (link = value->members)) {
	value->members = link->next;
	gql_value_destroy(link->value);
	DEBUG_ALLOC(mem_graphql_link, link);
	free(link);
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
	text = agoo_text_append(text, spaces, i2);
	text = link->value->type->to_json(text, link->value, indent, d2);
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = list_destroy,
    .to_json = list_to_json,
};

// Object, not visible but used for object values.
static void
object_destroy(gqlValue value) {
    gqlLink	link;

    while (NULL != (link = value->members)) {
	value->members = link->next;
	gql_value_destroy(link->value);
	free(link->key);
	DEBUG_ALLOC(mem_graphql_link, link);
	free(link);
    }
}

static agooText
object_to_json(agooText text, gqlValue value, int indent, int depth) {
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
	text = agoo_text_append(text, spaces, i2);
	text = agoo_text_append(text, "\"", 1);
	text = agoo_text_append(text, link->key, -1);
	if (0 < indent) {
	    text = agoo_text_append(text, "\": ", 3);
	} else {
	    text = agoo_text_append(text, "\":", 2);
	}
	text = link->value->type->to_json(text, link->value, indent, d2);
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
    .locked = true,
    .core = true,
    .coerce = NULL,
    .destroy = object_destroy,
    .to_json = object_to_json,
};

////////////////////////////////////////////////////////////////////////////////
void
gql_value_destroy(gqlValue value) {
    if (NULL != value->type->destroy) {
	value->type->destroy(value);
    }
    DEBUG_ALLOC(mem_graphql_value, value);
    free(value);
}

int
gql_value_init(agooErr err) {
    if (AGOO_ERR_OK != gql_type_set(err, &gql_int_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_i64_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_bool_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_float_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_time_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_uuid_type) ||
	AGOO_ERR_OK != gql_type_set(err, &gql_url_type) ||
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

void
gql_string_set(gqlValue value, const char *str, int len) {
    value->type = &gql_string_type;
    if (NULL == str) {
	value->str = NULL;
    } else {
	if (0 >= len) {
	    len = (int)strlen(str);
	}
	if (len < (int)sizeof(value->str16)) {
	    value->type = &gql_str16_type;
	    strncpy(value->str16, str, len);
	} else {
	    value->str = strndup(str, len);
	}
    }
}

int
gql_url_set(agooErr err, gqlValue value, const char *url, int len) {
    value->type = &gql_url_type;
    if (NULL == url) {
	value->url = NULL;
    } else {
	if (0 >= len) {
	    len = (int)strlen(url);
	}
	value->url = strndup(url, len);
    }
    return AGOO_ERR_OK;
}

void
gql_bool_set(gqlValue value, bool b) {
    value->b = b;
}

void
gql_float_set(gqlValue value, double f) {
    value->f = f;
}

void
gql_time_set(gqlValue value, int64_t t) {
    value->time = t;
}

int
gql_time_str_set(agooErr err, gqlValue value, const char *str, int len) {
    if (0 >= len) {
	len = (int)strlen(str);
    }
    value->time = time_parse(err, str, len);

    return err->code;
}

void
gql_uuid_set(gqlValue value, uint64_t hi, uint64_t lo) {
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
    value->uuid.hi = hi;
    value->uuid.lo = lo;

    return AGOO_ERR_OK;
}

extern void	gql_null_set(gqlValue value);

gqlLink
link_create(agooErr err, gqlValue item) {
    gqlLink	link = (gqlLink)malloc(sizeof(struct _gqlLink));

    if (NULL == link) {
	agoo_err_set(err, AGOO_ERR_MEMORY, "Failed to allocation memory for a list link.");
    } else {
	DEBUG_ALLOC(mem_graphql_link, link);
	link->next = NULL;
	link->key = NULL;
	link->value = item;
    }
    return link;
}

int
gql_list_append(agooErr err, gqlValue list, gqlValue item) {
    gqlLink	link = link_create(err, item);

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
    gqlLink	link = link_create(err, item);

    if (NULL != link) {
	link->next = list->members;
	list->members = link;
    }
    return AGOO_ERR_OK;
}

int
gql_object_set(agooErr err, gqlValue obj, const char *key, gqlValue item) {
    gqlLink	link = link_create(err, item);

    if (NULL != link) {
	link->key = strdup(key);
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
    gqlValue	v = (gqlValue)malloc(sizeof(struct _gqlValue));
    
    if (NULL != v) {
	DEBUG_ALLOC(mem_graphql_value, v);
	memset(v, 0, sizeof(struct _gqlValue));
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
    if ((int)sizeof(v->str16) <= len) {
	if (NULL != (v = value_create(&gql_string_type))) {
	    v->str = strndup(str, len);
	}
    } else {
	if (NULL != (v = value_create(&gql_str16_type))) {
	    strncpy(v->str16, str, len);
	    v->str16[len] = '\0';
	}
    }
    return v;
}

gqlValue
gql_url_create(agooErr err, const char *url, int len) {
    gqlValue	v = value_create(&gql_url_type);
    
    if (0 >= len) {
	len = (int)strlen(url);
    }
    if (NULL != v) {
	v->str = strndup(url, len);
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
gql_list_create(agooErr err, gqlType itemType) {
    gqlValue	v = value_create(&list_type);

    if (NULL != v) {
	v->members = NULL;
	v->member_type = itemType;
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
    return value->type->to_json(text, value, indent, depth);
}
