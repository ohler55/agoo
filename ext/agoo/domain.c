// Copyright 2019 by Peter Ohler, All Rights Reserved

#include <regex.h>

#include "debug.h"
#include "domain.h"

typedef struct _domain {
    struct _domain	*next;
    char		*host;
    char		*path;
    regex_t		*rhost;
    char		*rpath;
} *Domain;

static Domain	domains = NULL;

bool
agoo_domain_use() {
    return NULL != domains;
}

int
agoo_domain_add(agooErr err, const char *host, const char *path) {

    // TBD

    return AGOO_ERR_OK;
}

int
agoo_domain_add_regex(agooErr err, const char *host, const char *path) {

    // TBD

    return AGOO_ERR_OK;
}

const char*
agoo_domain_resolve(const char *host, char *buf, size_t blen) {

    // TBD

    return NULL;
}
