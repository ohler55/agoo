// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SUBJECT_H
#define AGOO_SUBJECT_H

#include <stdbool.h>

typedef struct _agooSubject {
    struct _agooSubject	*next;
    char		pattern[8];
} *agooSubject;

extern agooSubject	agoo_subject_create(const char *pattern, int plen);
extern void		agoo_subject_destroy(agooSubject subject);
extern bool		agoo_subject_check(agooSubject subj, const char *subject);

#endif // AGOO_SUBJECT_H
