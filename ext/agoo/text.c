// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#include <stdlib.h>
#include <string.h>

#include "text.h"

Text
text_create(const char *str, int len) {
    Text	t = (Text)malloc(sizeof(struct _Text) - TEXT_MIN_SIZE + len + 1);

    if (NULL != t) {
	t->len = len;
	t->alen = len;
	atomic_init(&t->ref_cnt, 0);
	memcpy(t->text, str, len);
	t->text[len] = '\0';
    }
    return t;
}

Text
text_allocate(int len) {
    Text	t = (Text)malloc(sizeof(struct _Text) - TEXT_MIN_SIZE + len + 1);

    if (NULL != t) {
	t->len = 0;
	t->alen = len;
	atomic_init(&t->ref_cnt, 0);
	*t->text = '\0';
    }
    return t;
}

void
text_ref(Text t) {
    atomic_fetch_add(&t->ref_cnt, 1);
}

void
text_release(Text t) {
    if (1 >= atomic_fetch_sub(&t->ref_cnt, 1)) {
	free(t);
    }
}

Text
text_append(Text t, const char *s, int len) {
    if (0 >= len) {
	len = strlen(s);
    }
    if (t->alen <= t->len + len) {
	long	new_len = t->alen + t->alen / 2;
	size_t	size = sizeof(struct _Text) - TEXT_MIN_SIZE + new_len + 1;

	if (NULL == (t = (Text)realloc(t, size))) {
	    return NULL;
	}
	t->alen = new_len;
    }
    memcpy(t->text + t->len, s, len);
    t->len += len;
    t->text[t->len] = '\0';

    return t;
}
