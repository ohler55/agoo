// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "text.h"

static const char	hex_chars[17] = "0123456789abcdef";

static char	json_chars[256] = "\
66666666222622666666666666666666\
11211111111111111111111111111111\
11111111111111111111111111112111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111\
11111111111111111111111111111111";

static void
dump_hex(char *s, uint8_t c) {
    uint8_t	d = (c >> 4) & 0x0F;

    *s++ = hex_chars[d];
    d = c & 0x0F;
    *s++ = hex_chars[d];
}

static size_t
json_size(const char *str, size_t len) {
    uint8_t	*u = (uint8_t*)str;
    size_t	size = 0;
    size_t	i = len;

    for (; 0 < i; u++, i--) {
	size += json_chars[*u];
    }
    return size - len * (size_t)'0';
}


agooText
agoo_text_create(const char *str, int len) {
    size_t	alen = (AGOO_TEXT_MIN_SIZE <= len) ? len : AGOO_TEXT_MIN_SIZE;
    agooText	t = (agooText)AGOO_MALLOC(sizeof(struct _agooText) + alen + 1);

    if (NULL != t) {
	t->next = NULL;
	t->len = len;
	t->alen = alen;
	t->bin = false;
	atomic_init(&t->ref_cnt, 0);
	memcpy(t->text, str, len);
	t->text[len] = '\0';
    }
    return t;
}

agooText
agoo_text_dup(agooText t0) {
    agooText	t = NULL;

    if (NULL != t0) {
	if (NULL != (t = (agooText)AGOO_MALLOC(sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + t0->alen + 1))) {
	    t->next = NULL;
	    t->len = t0->len;
	    t->alen = t0->alen;
	    t->bin = false;
	    atomic_init(&t->ref_cnt, 0);
	    memcpy(t->text, t0->text, t0->len + 1);
	}
    }
    return t;
}

agooText
agoo_text_allocate(int len) {
    size_t	alen = (AGOO_TEXT_MIN_SIZE <= len) ? len : AGOO_TEXT_MIN_SIZE;
    agooText	t = (agooText)AGOO_MALLOC(sizeof(struct _agooText) + alen + 1);

    if (NULL != t) {
	t->next = NULL;
	t->len = 0;
	t->alen = alen;
	t->bin = false;
	atomic_init(&t->ref_cnt, 0);
	*t->text = '\0';
    }
    return t;
}

void
agoo_text_ref(agooText t) {
    atomic_fetch_add(&t->ref_cnt, 1);
}

void
agoo_text_release(agooText t) {
    if (1 >= atomic_fetch_sub(&t->ref_cnt, 1)) {
	AGOO_FREE(t);
    }
}

agooText
agoo_text_append(agooText t, const char *s, int len) {
    if (NULL == t) {
	return NULL;
    }
    if (0 >= len) {
	len = (int)strlen(s);
    }
    if (t->alen <= t->len + len) {
	long	new_len = t->alen + len + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;

	if (NULL == (t = (agooText)AGOO_REALLOC(t, size))) {
	    return NULL;
	}
	t->alen = new_len;
    }
    memcpy(t->text + t->len, s, len);
    t->len += len;
    t->text[t->len] = '\0';

    return t;
}

agooText
agoo_text_append_char(agooText t, const char c) {
    if (NULL == t) {
	return NULL;
    }
    if (t->alen <= t->len + 1) {
	long	new_len = t->alen + 1 + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;

	if (NULL == (t = (agooText)AGOO_REALLOC(t, size))) {
	    return NULL;
	}
	t->alen = new_len;
    }
    *(t->text + t->len) = c;
    t->len++;
    t->text[t->len] = '\0';

    return t;
}

agooText
agoo_text_prepend(agooText t, const char *s, int len) {
    if (NULL == t) {
	return NULL;
    }
    if (0 >= len) {
	len = (int)strlen(s);
    }
    if (t->alen <= t->len + len) {
	long	new_len = t->alen + len + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;

	if (NULL == (t = (agooText)AGOO_REALLOC(t, size))) {
	    return NULL;
	}
	t->alen = new_len;
    }
    memmove(t->text + len, t->text, t->len + 1);
    memcpy(t->text, s, len);
    t->len += len;

    return t;
}

agooText
agoo_text_append_json(agooText t, const char *s, int len) {
    size_t	jlen;

    if (NULL == t) {
	return NULL;
    }
    if (0 >= len) {
	len = (int)strlen(s);
    }
    jlen = json_size(s, len);
    if (t->alen <= (long)(t->len + jlen)) {
	long	new_len = t->alen + jlen + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;

	if (NULL == (t = (agooText)AGOO_REALLOC(t, size))) {
	    return NULL;
	}
	t->alen = new_len;
    }
    if (jlen == (size_t)len) {
	memcpy(t->text + t->len, s, len);
    } else {
	const char	*end = s + len;
	char		*ts = t->text + t->len;

	for (; s < end; s++) {
	    switch (json_chars[(uint8_t)*s]) {
	    case '1':
		*ts++ = *s;
		break;
	    case '2':
		*ts++ = '\\';
		switch (*s) {
		case '\\':	*ts++ = '\\';	break;
		case '\b':	*ts++ = 'b';	break;
		case '\t':	*ts++ = 't';	break;
		case '\n':	*ts++ = 'n';	break;
		case '\f':	*ts++ = 'f';	break;
		case '\r':	*ts++ = 'r';	break;
		default:	*ts++ = *s;	break;
		}
		break;
	    case '6': // control characters
		*ts++ = '\\';
		*ts++ = 'u';
		*ts++ = '0';
		*ts++ = '0';
		dump_hex(ts, (uint8_t)*s);
		ts += 2;
		break;
	    default:
		break; // should never get here
	    }
	}
    }
    t->len += jlen;
    t->text[t->len] = '\0';

    return t;
}

void
agoo_text_reset(agooText t) {
    t->len = 0;
}
