// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_TEXT_H
#define AGOO_TEXT_H

#include <stdbool.h>

#include "atomic.h"

#define AGOO_TEXT_MIN_SIZE	8

typedef struct _agooText {
    long	len; // length of valid text
    long	alen; // size of allocated text
    atomic_int	ref_cnt;
    bool	bin;
    char	text[AGOO_TEXT_MIN_SIZE];
} *agooText;

extern agooText	text_create(const char *str, int len);
extern agooText	text_dup(agooText t);
extern agooText	text_allocate(int len);
extern void	text_ref(agooText t);
extern void	text_release(agooText t);
extern agooText	text_append(agooText t, const char *s, int len);
extern agooText	text_prepend(agooText t, const char *s, int len);

#endif // AGOO_TEXT_H
