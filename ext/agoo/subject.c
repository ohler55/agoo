// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "subject.h"

agooSubject
agoo_subject_create(const char *pattern, int plen) {
    agooSubject	subject = (agooSubject)AGOO_MALLOC(sizeof(struct _agooSubject) - 7 + plen);

    if (NULL != subject) {
	subject->next = NULL;
	memcpy(subject->pattern, pattern, plen);
	subject->pattern[plen] = '\0';
    }
    return subject;
}

void
agoo_subject_destroy(agooSubject subject) {
    AGOO_FREE(subject);
}

bool
agoo_subject_check(agooSubject subj, const char *subject) {
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

