// Copyright 2011, 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_BASE64_H
#define AGOO_BASE64_H

typedef unsigned char	uchar;

#define b64_size(len) ((len + 2) / 3 * 4)

extern unsigned long    b64_orig_size(const char *text);

extern int	        b64_to(const uchar *src, int len, char *b64);
extern void             b64_from(const char *b64, uchar *str);

#endif // AGOO_BASE64_H
