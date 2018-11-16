// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "text.h"

agooText
text_create(const char *str, int len) {
    agooText	t = (agooText)malloc(sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + len + 1);

    if (NULL != t) {
	DEBUG_ALLOC(mem_text, t)
	t->len = len;
	t->alen = len;
	t->bin = false;
	atomic_init(&t->ref_cnt, 0);
	memcpy(t->text, str, len);
	t->text[len] = '\0';
    }
    return t;
}

agooText
text_dup(agooText t0) {
    agooText	t = (agooText)malloc(sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + t0->alen + 1);

    if (NULL != t) {
	DEBUG_ALLOC(mem_text, t)
	t->len = t0->len;
	t->alen = t0->alen;
	t->bin = false;
	atomic_init(&t->ref_cnt, 0);
	memcpy(t->text, t0->text, t0->len + 1);
    }
    return t;
}

agooText
text_allocate(int len) {
    agooText	t = (agooText)malloc(sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + len + 1);

    if (NULL != t) {
	DEBUG_ALLOC(mem_text, t)
	t->len = 0;
	t->alen = len;
	t->bin = false;
	atomic_init(&t->ref_cnt, 0);
	*t->text = '\0';
    }
    return t;
}

void
text_ref(agooText t) {
    atomic_fetch_add(&t->ref_cnt, 1);
}

void
text_release(agooText t) {
    if (1 >= atomic_fetch_sub(&t->ref_cnt, 1)) {
	DEBUG_FREE(mem_text, t);
	free(t);
    }
}

agooText
text_append(agooText t, const char *s, int len) {
    if (0 >= len) {
	len = (int)strlen(s);
    }
    if (t->alen <= t->len + len) {
	long	new_len = t->alen + len + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;
#ifdef MEM_DEBUG
	agooText	t0 = t;
#endif	
	if (NULL == (t = (agooText)realloc(t, size))) {
	    return NULL;
	}
	DEBUG_REALLOC(mem_text, t0, t);
	t->alen = new_len;
    }
    memcpy(t->text + t->len, s, len);
    t->len += len;
    t->text[t->len] = '\0';

    return t;
}

agooText
text_prepend(agooText t, const char *s, int len) {
    if (0 >= len) {
	len = (int)strlen(s);
    }
    if (t->alen <= t->len + len) {
	long	new_len = t->alen + len + t->alen / 2;
	size_t	size = sizeof(struct _agooText) - AGOO_TEXT_MIN_SIZE + new_len + 1;
#ifdef MEM_DEBUG
	agooText	t0 = t;
#endif	

	if (NULL == (t = (agooText)realloc(t, size))) {
	    return NULL;
	}
	DEBUG_REALLOC(mem_text, t0, t);
	t->alen = new_len;
    }
    memmove(t->text + len, t->text, t->len + 1);
    memcpy(t->text, s, len);
    t->len += len;

    return t;
}
