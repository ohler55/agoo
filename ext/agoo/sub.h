// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SUB_H
#define AGOO_SUB_H

#include <stdbool.h>
#include <stdint.h>

#define SUB_BUCKET_SIZE	1024
#define SUB_BUCKET_MASK	1023

struct _Con;

typedef struct _Sub {
    struct _Sub	*next;
    uint64_t	cid;
    struct _Con	*con;
    uint64_t	id;
    uint64_t	key; // hash key
    char	subject[8];
} *Sub;

typedef struct _SubCache {
    Sub	buckets[SUB_BUCKET_SIZE];
} *SubCache;

extern void	sub_init(SubCache sc);
extern void	sub_cleanup(SubCache sc);

extern Sub	sub_new(uint64_t cid, uint64_t id, const char *subject, size_t slen);
extern bool	sub_match(Sub s, const char *subject, size_t slen);
extern void	sub_add(SubCache sc, Sub s);
extern Sub	sub_get(SubCache sc, uint64_t cid, uint64_t sid);
extern void	sub_del(SubCache sc, uint64_t cid, uint64_t sid);

#endif // AGOO_SUB_H
