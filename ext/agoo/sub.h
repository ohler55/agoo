// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SUB_H
#define AGOO_SUB_H

#include <stdbool.h>
#include <stdint.h>

#define SUB_BUCKET_SIZE	1024
#define SUB_BUCKET_MASK	1023

struct _agooCon;

typedef struct _agooSub {
    struct _agooSub	*next;
    uint64_t		cid;
    struct _agooCon	*con;
    uint64_t		id;
    uint64_t		key; // hash key
    char		subject[8];
} *agooSub;

typedef struct _agooSubCache {
    agooSub	buckets[SUB_BUCKET_SIZE];
} *agooSubCache;

extern void	sub_init(agooSubCache sc);
extern void	sub_cleanup(agooSubCache sc);

extern agooSub	sub_new(uint64_t cid, uint64_t id, const char *subject, size_t slen);
extern bool	sub_match(agooSub s, const char *subject, size_t slen);
extern void	sub_add(agooSubCache sc, agooSub s);
extern agooSub	sub_get(agooSubCache sc, uint64_t cid, uint64_t sid);
extern void	sub_del(agooSubCache sc, uint64_t cid, uint64_t sid);

#endif // AGOO_SUB_H
