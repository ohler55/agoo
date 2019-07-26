// Copyright 2019 by Peter Ohler, All Rights Reserved

#include <inttypes.h>
#include <regex.h>
#include <stdio.h>

#include "debug.h"
#include "domain.h"

#define MAX_MATCH	8

typedef struct _domain {
    struct _domain	*next;
    char		*host;
    char		*path;
    regex_t		rhost;
} *Domain;

static Domain	domains = NULL;

bool
agoo_domain_use() {
    return NULL != domains;
}

int
agoo_domain_add(agooErr err, const char *host, const char *path) {
    Domain	d = (Domain)AGOO_CALLOC(1, sizeof(struct _domain));

    if (NULL == d) {
	return AGOO_ERR_MEM(err, "Domain");
    }
    if (NULL == (d->host = AGOO_STRDUP(host))) {
	return AGOO_ERR_MEM(err, "Domain host");
    }
    if (NULL == (d->path = AGOO_STRDUP(path))) {
	return AGOO_ERR_MEM(err, "Domain path");
    }
    if (NULL == domains) {
	domains = d;
    } else {
	Domain	last = domains;

	for (; NULL != last->next; last = last->next) {
	}
	last->next = d;
    }
    return AGOO_ERR_OK;
}

int
agoo_domain_add_regex(agooErr err, const char *host, const char *path) {
    Domain	d = (Domain)AGOO_CALLOC(1, sizeof(struct _domain));

    if (NULL == d) {
	return AGOO_ERR_MEM(err, "Domain");
    }
    if (0 != regcomp(&d->rhost, host, REG_EXTENDED | REG_NEWLINE)) {
	return agoo_err_set(err, AGOO_ERR_ARG, "invalid regex");
    }
    if (NULL == (d->path = AGOO_STRDUP(path))) {
	return AGOO_ERR_MEM(err, "Domain path");
    }
    if (NULL == domains) {
	domains = d;
    } else {
	Domain	last = domains;

	for (; NULL != last->next; last = last->next) {
	}
	last->next = d;
    }
    return AGOO_ERR_OK;
}

const char*
agoo_domain_resolve(const char *host, char *buf, size_t blen) {
    Domain	d;

    for (d = domains; NULL != d; d = d->next) {
	if (NULL != d->host) { // simple string compare
	    if (0 == strcmp(host, d->host)) {
		return d->path;
	    }
	} else {
	    regmatch_t	matches[MAX_MATCH];
	    char	*bend = buf + blen - 1;

	    if (0 == regexec(&d->rhost, host, MAX_MATCH, matches, 0)) {
		char	*b = buf;
		char	*p = d->path;

		for (; '\0' != *p; p++) {
		    if ('$' == *p && '(' == *(p + 1)) {
			const char	*m;
			char		*end;
			long		i;
			int		start;
			int		len;

			p += 2;
			i = strtol(p, &end, 10);
			if (')' != *end || MAX_MATCH <= i) {
			    continue;
			}
			p = end;
			if (0 > (start = (int)matches[i].rm_so)) {
			    continue;
			}
			len = (int)matches[i].rm_eo - start;
			if (bend - b <= len) {
			    continue;
			}
			for (m = host + start; 0 < len; len--) {
			    *b++ = *m++;
			    *b = '\0';
			}
			// TBD
		    } else {
			*b++ = *p;
		    }
		}
		*b = '\0';
		return buf;
	    }
	}
    }
    return NULL;
}

void
agoo_domain_cleanup() {
    Domain	d;

    while (NULL != (d = domains)) {
	domains = d->next;
	if (NULL == d->host) {
	    regfree(&d->rhost);
	} else {
	    AGOO_FREE(d->host);
	}
	AGOO_FREE(d->path);
	AGOO_FREE(d);
    }
}
