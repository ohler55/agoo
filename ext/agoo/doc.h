// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_DOC_H
#define AGOO_DOC_H

#include <stdarg.h>

#include "err.h"

struct _gqlValue;

typedef struct _Doc {
    const char	*str;
    const char	*cur;
    const char	*end;
} *Doc;

extern void	doc_init(Doc doc, const char *str, int len);

extern int	doc_skip_white(Doc doc);
extern void	doc_skip_comment(Doc doc);
extern int	doc_read_desc(Err err, Doc doc);

extern void	doc_next_token(Doc doc);
extern void	doc_read_token(Doc doc);

extern int	doc_read_string(Err err, Doc doc);

extern int	doc_err(Doc doc, Err err, const char *fmt, ...);
extern void	doc_location(Doc doc, int *linep, int *colp);

extern struct _gqlValue*	doc_read_value(Err err, Doc doc);

#endif // AGOO_DOC_H
