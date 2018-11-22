// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_DOC_H
#define AGOO_DOC_H

#include <stdarg.h>

#include "err.h"

struct _gqlValue;

typedef struct _agooDoc {
    const char	*str;
    const char	*cur;
    const char	*end;
} *agooDoc;

extern void	agoo_doc_init(agooDoc doc, const char *str, int len);

extern int	agoo_doc_skip_white(agooDoc doc);
extern void	agoo_doc_skip_comment(agooDoc doc);
extern int	agoo_doc_read_desc(agooErr err, agooDoc doc);

extern void	agoo_doc_next_token(agooDoc doc);
extern void	agoo_doc_read_token(agooDoc doc);

extern int	agoo_doc_read_string(agooErr err, agooDoc doc);

extern int	agoo_doc_err(agooDoc doc, agooErr err, const char *fmt, ...);
extern void	agoo_doc_location(agooDoc doc, int *linep, int *colp);

extern struct _gqlValue*	agoo_doc_read_value(agooErr err, agooDoc doc);

#endif // AGOO_DOC_H
