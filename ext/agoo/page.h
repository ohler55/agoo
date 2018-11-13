// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_PAGE_H
#define AGOO_PAGE_H

#include <stdint.h>
#include <time.h>

#include "err.h"
#include "text.h"

typedef struct _Page {
    Text		resp;
    char		*path;
    time_t		mtime;
    double		last_check;
    bool		immutable;
} *Page;

typedef struct _Dir {
    struct _Dir		*next;
    char		*path;
    int			plen;
} *Dir;

typedef struct _Group {
    struct _Group	*next;
    char		*path;
    int			plen;
    Dir			dirs;
} *Group;

extern void	pages_init();
extern void	pages_set_root(const char *root);
extern void	pages_cleanup();

extern Group	group_create(const char *path);
extern void	group_add(Group g, const char *dir);
extern Page	group_get(Err err, const char *path, int plen);

extern Page	page_create(const char *path);
extern Page	page_immutable(Err err, const char *path, const char *content, int clen);
extern Page	page_get(Err err, const char *path, int plen);
extern int	mime_set(Err err, const char *key, const char *value);

#endif /* AGOO_PAGE_H */
