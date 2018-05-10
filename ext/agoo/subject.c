// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "subject.h"

Subject
subject_create(const char *pattern, int plen) {
    Subject	subject = (Subject)malloc(sizeof(struct _Subject) - 7 + plen);

    if (NULL != subject) {
	DEBUG_ALLOC(mem_subject, subject);
	subject->next = NULL;
	memcpy(subject->pattern, pattern, plen);
	subject->pattern[plen] = '\0';
    }
    return subject;
}

void
subject_destroy(Subject subject) {
    DEBUG_FREE(mem_subject, subject);
    free(subject);
}

bool
subject_check(Subject subj, const char *subject) {
    const char	*pat = subj->pattern;

    for (; '\0' != *pat && '\0' != *subject; subject++) {
	if (*subject == *pat) {
	    pat++;
	} else if ('*' == *pat) {
	    for (; '\0' != *subject && '.' != *subject; subject++) {
	    }
	    if ('\0' == *subject) {
		return true;
	    }
	    pat++;
	} else if ('>' == *pat) {
	    return true;
	} else {
	    break;
	}
    }
    return '\0' == *pat && '\0' == *subject;
}

