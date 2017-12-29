// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef __AGOO_TEXT_H__
#define __AGOO_TEXT_H__

#include <stdatomic.h>
#include <stdbool.h>

#define TEXT_MIN_SIZE	8

typedef struct _Text {
    long	len; // length of valid text
    long	alen; // size of allocated text
    atomic_int	ref_cnt;
    char	text[TEXT_MIN_SIZE];
} *Text;

extern Text	text_create(const char *str, int len);
extern Text	text_allocate(int len);
extern void	text_ref(Text t);
extern void	text_release(Text t);
extern Text	text_append(Text t, const char *s, int len);

#endif /* __AGOO_TEXT_H__ */
