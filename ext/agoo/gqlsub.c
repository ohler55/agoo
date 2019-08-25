// Copyright (c) 2019, Peter Ohler, All rights reserved.

#include "con.h"
#include "debug.h"
#include "gqlsub.h"
#include "graphql.h"

gqlSub
gql_sub_create(agooErr err, agooCon con, const char *subject, struct _gqlDoc *query) {
    gqlSub	sub = (gqlSub)AGOO_MALLOC(sizeof(struct _gqlSub));

    if (NULL == sub) {
	AGOO_ERR_MEM(err, "gqlSub");
	return NULL;
    }
    if (NULL == (sub->subject = AGOO_STRDUP(subject))) {
	AGOO_ERR_MEM(err, "strdup()");
	AGOO_FREE(sub);
	return NULL;
    }
    sub->next = NULL;
    sub->con = con;
    con->gsub = sub;
    sub->query = query;

    return sub;

}

void
gql_sub_destroy(gqlSub sub) {
    AGOO_FREE(sub->subject);
    if (NULL != sub->query) {
	gql_doc_destroy(sub->query);
    }
    AGOO_FREE(sub);
}
