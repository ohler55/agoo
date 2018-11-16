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
pub_close(agooUpgraded up) {
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_CLOSE;
	p->up = up;
	p->subject = NULL;
	p->msg = NULL;
    }
    return p;
}

agooPub
pub_subscribe(agooUpgraded up, const char *subject, int slen) {
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_SUB;
	p->up = up;
	p->subject = subject_create(subject, slen);
	p->msg = NULL;
    }
    return p;
}

agooPub
pub_unsubscribe(agooUpgraded up, const char *subject, int slen) {
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_UN;
	p->up = up;
	if (NULL != subject) {
	    p->subject = subject_create(subject, slen);
	} else {
	    p->subject = NULL;
	}
	p->msg = NULL;
    }
    return p;
}

agooPub
pub_publish(const char *subject, int slen, const char *message, size_t mlen) {
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_MSG;
	p->up = NULL;
	p->subject = subject_create(subject, slen);
	// Allocate an extra 24 bytes so the message can be expanded in place
	// if a WebSocket or SSE write.
	p->msg = text_append(text_allocate((int)mlen + 24), message, (int)mlen);
	text_ref(p->msg);
    }
    return p;
}

agooPub
pub_write(agooUpgraded up, const char *message, size_t mlen, bool bin) {
    // Allocate an extra 16 bytes so the message can be expanded in place if a
    // WebSocket write.
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_WRITE;
	p->up = up;
	p->subject = NULL;
	// Allocate an extra 16 bytes so the message can be expanded in place
	// if a WebSocket write.
	p->msg = text_append(text_allocate((int)mlen + 16), message, (int)mlen);
	p->msg->bin = bin;
	text_ref(p->msg);
    }
    return p;
}

agooPub
pub_dup(agooPub src) {
    agooPub	p = (agooPub)malloc(sizeof(struct _agooPub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = src->kind;
	p->up = src->up;
	p->subject = subject_create(src->subject->pattern, strlen(src->subject->pattern));
	p->msg = src->msg;
	text_ref(p->msg);
    }
    return p;
}

void
pub_destroy(agooPub pub) {
    if (NULL != pub->msg) {
	text_release(pub->msg);
    }
    if (NULL != pub->subject) {
	subject_destroy(pub->subject);
    }
    if (NULL != pub->up) {
	upgraded_release(pub->up);
    }
    DEBUG_FREE(mem_pub, pub);
    free(pub);
}

