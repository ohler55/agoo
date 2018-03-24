// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "sub.h"

Sub
sub_new(uint64_t cid, uint64_t id, const char *subject, size_t slen) {
    Sub	s = (Sub)malloc(sizeof(struct _Sub) - 8 + slen + 1);

    if (NULL != s) {
	s->cid = cid;
	s->id = id;
	strncpy(s->subject, subject, slen);
	s->subject[slen] = '\0';
    }
    return s;
}

bool
sub_match(Sub s, const char *subject, size_t slen) {
    const char	*pat = s->subject;
    const char	*end = subject + slen;

    for (; '\0' != *pat && subject < end; subject++) {
	if (*subject == *pat) {
	    subject++;
	} else if ('*' == *pat) {
	    for (; subject < end && '.' != *subject; subject++) {
	    }
	} else if ('>' == *pat) {
	    return true;
	} else {
	    break;
	}
    }
    return '\0' == *pat && subject == end;
}

void
sub_init(SubCache sc) {
    memset(sc, 0, sizeof(struct _SubCache));
}

void
sub_cleanup(SubCache sc) {
    Sub	s;
    Sub	head;
    Sub	*bucket;
    int	i;
    
    for (i = 0, bucket = sc->buckets; i < SUB_BUCKET_SIZE; i++, bucket++) {
	head = *bucket;
	while (NULL != head) {
	    s = head;
	    head = head->next;
	    free(s);
	}
    }
}

static uint64_t
calc_hash(uint64_t cid, uint64_t sid) {
    return (SUB_BUCKET_MASK & (cid ^ sid));
}

void
sub_add(SubCache sc, Sub s) {
    Sub	*bucket = sc->buckets + calc_hash(s->cid, s->id);

    s->next = *bucket;
    *bucket = s;
}

Sub
sub_get(SubCache sc, uint64_t cid, uint64_t sid) {
    Sub	s = *(sc->buckets + calc_hash(cid, sid));
    
    for (; NULL != s; s = s->next) {
	if (s->cid == cid && s->id == sid) {
	    return s;
	}
    }
    return NULL;
}

void
sub_del(SubCache sc, uint64_t cid, uint64_t sid) {
    Sub	*bucket = sc->buckets + calc_hash(cid, sid);

    if (NULL != *bucket) {
	Sub	s = *bucket;

	if (s->cid == cid && s->id == sid) {
	    *bucket = s->next;
	} else {
	    Sub	prev = s;
	    
	    for (s = s->next; NULL != s; s = s->next) {
		if (s->cid == cid && s->id == sid) {
		    prev->next = s->next;
		    free(s);
		    break;
		}
		prev = s;
	    }
	}
    }
}

