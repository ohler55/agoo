// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "pub.h"
#include "subject.h"
#include "text.h"
#include "upgraded.h"

Pub
pub_close(Upgraded up) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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

Pub
pub_subscribe(Upgraded up, const char *subject, int slen) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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

Pub
pub_unsubscribe(Upgraded up, const char *subject, int slen) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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

Pub
pub_publish(const char *subject, int slen, const char *message, size_t mlen) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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

Pub
pub_write(Upgraded up, const char *message, size_t mlen, bool bin) {
    // Allocate an extra 16 bytes so the message can be expanded in place if a
    // WebSocket write.
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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

Pub
pub_dup(Pub src) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

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
pub_destroy(Pub pub) {
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

