// Copyright 2019 by Peter Ohler, All Rights Reserved

#ifndef AGOO_DOMAIN_H
#define AGOO_DOMAIN_H

#include <stdbool.h>

#include "err.h"

extern bool		agoo_domain_use();
extern int		agoo_domain_add(agooErr err, const char *host, const char *path);
extern int		agoo_domain_add_regex(agooErr err, const char *host, const char *path);
extern const char*	agoo_domain_resolve(const char *host, char *buf, size_t blen);
extern void		agoo_domain_cleanup();

#endif // AGOO_DOMAIN_H
