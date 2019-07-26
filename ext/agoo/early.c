// Copyright (c) 2019, Peter Ohler, All rights reserved.

#include "debug.h"
#include "early.h"

agooEarly
agoo_early_create(const char *link) {
    agooEarly	early = (agooEarly)AGOO_CALLOC(1, sizeof(struct _agooEarly));

    if (NULL != early) {
	if (NULL == (early->link = AGOO_STRDUP(link))) {
	    AGOO_FREE(early);
	    return NULL;
	}
    }
    return early;
}

void
agoo_early_destroy(agooEarly early) {
    if (NULL != early) {
	if (NULL != early->next) {
	    agoo_early_destroy(early->next);
	}
	AGOO_FREE(early->link);
	AGOO_FREE(early);
    }
}
