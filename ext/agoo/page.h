// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef __AGOO_PAGE_H__
#define __AGOO_PAGE_H__

#include <stdint.h>
#include <time.h>

#include "err.h"
#include "text.h"

#define MAX_KEY_LEN		1024
#define PAGE_BUCKET_SIZE	1024
#define PAGE_BUCKET_MASK	1023

#define MAX_MIME_KEY_LEN	15
#define MIME_BUCKET_SIZE	64
#define MIME_BUCKET_MASK	63

typedef struct _Page {
    Text		resp;
    char		*path;
    time_t		mtime;
    double		last_check;
} *Page;

typedef struct _Slot {
    struct _Slot	*next;
    char		key[MAX_KEY_LEN + 1];
    Page		value;
    uint64_t		hash;
    int			klen;
} *Slot;

typedef struct _MimeSlot {
    struct _MimeSlot	*next;
    char		key[MAX_MIME_KEY_LEN + 1];
    char		*value;
    uint64_t		hash;
    int			klen;
} *MimeSlot;

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

typedef struct _Cache {
    Slot		buckets[PAGE_BUCKET_SIZE];
    MimeSlot		muckets[MIME_BUCKET_SIZE];
    char		*root;
    Group		groups;
} *Cache;

extern void	pages_init(Cache cache);
extern void	pages_cleanup(Cache cache);

extern Group	group_create(Cache cache, const char *path);
extern void	group_add(Group g, const char *dir);
extern Page	group_get(Err err, Cache cache, const char *path, int plen);

extern void	page_destroy(Page p);
extern Page	page_get(Err err, Cache cache, const char *path, int plen);
extern int	mime_set(Err err, Cache cache, const char *key, const char *value);

#endif /* __AGOO_PAGE_H__ */
