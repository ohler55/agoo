// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_PAGE_H
#define AGOO_PAGE_H

#include <stdint.h>
#include <time.h>

#include "err.h"
#include "text.h"

typedef struct _agooPage {
    agooText		resp;
    char		*path;
    time_t		mtime;
    double		last_check;
    bool		immutable;
} *agooPage;

typedef struct _agooDir {
    struct _agooDir	*next;
    char		*path;
    int			plen;
} *agooDir;

typedef struct _agooGroup {
    struct _agooGroup	*next;
    char		*path;
    int			plen;
    agooDir		dirs;
} *agooGroup;

extern void		pages_init();
extern void		pages_set_root(const char *root);
extern void		pages_cleanup();

extern agooGroup	group_create(const char *path);
extern void		group_add(agooGroup g, const char *dir);
extern agooPage		group_get(agooErr err, const char *path, int plen);

extern agooPage		page_create(const char *path);
extern agooPage		page_immutable(agooErr err, const char *path, const char *content, int clen);
extern agooPage		page_get(agooErr err, const char *path, int plen);
extern int		mime_set(agooErr err, const char *key, const char *value);

#endif // AGOO_PAGE_H
