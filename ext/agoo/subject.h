// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SUBJECT_H
#define AGOO_SUBJECT_H

#include <stdbool.h>

typedef struct _agooSubject {
    struct _agooSubject	*next;
    char		pattern[8];
} *agooSubject;

extern agooSubject	subject_create(const char *pattern, int plen);
extern void	subject_destroy(agooSubject subject);
extern bool	subject_check(agooSubject subj, const char *subject);

#endif // AGOO_SUBJECT_H
