// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_TEXT_H
#define AGOO_TEXT_H

#include <stdatomic.h>
#include <stdbool.h>

#define TEXT_MIN_SIZE	8

typedef struct _Text {
    long	len; // length of valid text
    long	alen; // size of allocated text
    atomic_int	ref_cnt;
    bool	bin;
    char	text[TEXT_MIN_SIZE];
} *Text;

extern Text	text_create(const char *str, int len);
extern Text	text_dup(Text t);
extern Text	text_allocate(int len);
extern void	text_ref(Text t);
extern void	text_release(Text t);
extern Text	text_append(Text t, const char *s, int len);
extern Text	text_prepend(Text t, const char *s, int len);

#endif /* AGOO_TEXT_H */
