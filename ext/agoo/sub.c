// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "sub.h"

agooSub
sub_new(uint64_t cid, uint64_t id, const char *subject, size_t slen) {
    agooSub	s = (agooSub)malloc(sizeof(struct _agooSub) - 8 + slen + 1);

    if (NULL != s) {
	s->cid = cid;
	s->id = id;
	strncpy(s->subject, subject, slen);
	s->subject[slen] = '\0';
    }
    return s;
}

bool
sub_match(agooSub s, const char *subject, size_t slen) {
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
sub_init(agooSubCache sc) {
    memset(sc, 0, sizeof(struct _agooSubCache));
}

void
sub_cleanup(agooSubCache sc) {
    agooSub	s;
    agooSub	head;
    agooSub	*bucket;
    int		i;
    
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
sub_add(agooSubCache sc, agooSub s) {
    agooSub	*bucket = sc->buckets + calc_hash(s->cid, s->id);

    s->next = *bucket;
    *bucket = s;
}

agooSub
sub_get(agooSubCache sc, uint64_t cid, uint64_t sid) {
    agooSub	s = *(sc->buckets + calc_hash(cid, sid));
    
    for (; NULL != s; s = s->next) {
	if (s->cid == cid && s->id == sid) {
	    return s;
	}
    }
    return NULL;
}

void
sub_del(agooSubCache sc, uint64_t cid, uint64_t sid) {
    agooSub	*bucket = sc->buckets + calc_hash(cid, sid);

    if (NULL != *bucket) {
	agooSub	s = *bucket;

	if (s->cid == cid && s->id == sid) {
	    *bucket = s->next;
	} else {
	    agooSub	prev = s;
	    
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

