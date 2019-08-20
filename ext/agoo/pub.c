// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "pub.h"
#include "subject.h"
#include "text.h"
#include "upgraded.h"

agooPub
agoo_pub_close(agooUpgraded up) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = AGOO_PUB_CLOSE;
	p->up = up;
	p->subject = NULL;
	p->msg = NULL;
    }
    return p;
}

agooPub
agoo_pub_subscribe(agooUpgraded up, const char *subject, int slen) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = AGOO_PUB_SUB;
	p->up = up;
	p->subject = agoo_subject_create(subject, slen);
	p->msg = NULL;
    }
    return p;
}

agooPub
agoo_pub_unsubscribe(agooUpgraded up, const char *subject, int slen) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = AGOO_PUB_UN;
	p->up = up;
	if (NULL != subject) {
	    p->subject = agoo_subject_create(subject, slen);
	} else {
	    p->subject = NULL;
	}
	p->msg = NULL;
    }
    return p;
}

agooPub
agoo_pub_publish(const char *subject, int slen, const char *message, size_t mlen) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = AGOO_PUB_MSG;
	p->up = NULL;
	p->subject = agoo_subject_create(subject, slen);
	// Allocate an extra 32 bytes so the message can be expanded in place
	// if a WebSocket or SSE write.
	p->msg = agoo_text_append(agoo_text_allocate((int)mlen + 32), message, (int)mlen);
	agoo_text_ref(p->msg);
    }
    return p;
}

agooPub
agoo_pub_write(agooUpgraded up, const char *message, size_t mlen, bool bin) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = AGOO_PUB_WRITE;
	p->up = up;
	p->subject = NULL;
	// Allocate an extra 32 bytes so the message can be expanded in place
	// if a WebSocket or SSE write.
	p->msg = agoo_text_append(agoo_text_allocate((int)mlen + 32), message, (int)mlen);
	p->msg->bin = bin;
	agoo_text_ref(p->msg);
    }
    return p;
}

agooPub
agoo_pub_dup(agooPub src) {
    agooPub	p = (agooPub)AGOO_MALLOC(sizeof(struct _agooPub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = src->kind;
	p->up = src->up;
	if (NULL != p->up) {
	    agoo_upgraded_ref(p->up);
	}
	if (NULL == src->subject) {
	    p->subject = NULL;
	} else {
	    p->subject = agoo_subject_create(src->subject->pattern, strlen(src->subject->pattern));
	}
	p->msg = src->msg;
	if (NULL != p->msg) {
	    agoo_text_ref(p->msg);
	}
    }
    return p;
}

void
agoo_pub_destroy(agooPub pub) {
    if (NULL != pub->msg) {
	agoo_text_release(pub->msg);
    }
    if (NULL != pub->subject) {
	agoo_subject_destroy(pub->subject);
    }
    if (NULL != pub->up) {
	agoo_upgraded_release(pub->up);
    }
    AGOO_FREE(pub);
}
