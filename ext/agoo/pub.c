// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "pub.h"
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
	p->sid = 0;
	p->subject = NULL;
    }
    return p;
}

Pub
pub_subscribe(Upgraded up, uint64_t sid, const char *subject) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_SUB;
	p->up = up;
	p->sid = sid;
	p->subject = strdup(subject);
    }
    return p;
}

Pub
pub_unsubscribe(Upgraded up, uint64_t sid) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_UN;
	p->up = up;
	p->sid = sid;
	p->subject = NULL;
    }
    return p;
}

Pub
pub_publish(char *subject, const char *message, size_t mlen, bool bin) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	DEBUG_ALLOC(mem_pub, p);
	p->next = NULL;
	p->kind = PUB_MSG;
	p->up = NULL;
	p->subject = strdup(subject);
	// Allocate an extra 24 bytes so the message can be expanded in place
	// if a WebSocket or SSE write.
	p->msg = text_allocate((int)mlen + 24);
	p->msg = text_append(text_allocate((int)mlen + 16), message, (int)mlen);
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

void
pub_destroy(Pub pub) {
    switch (pub->kind) {
    case PUB_MSG:
    case PUB_WRITE:
	if (NULL != pub->msg) {
	    text_release(pub->msg);
	}
	break;
    default:
	break;
    }
    upgraded_release(pub->up);
    DEBUG_FREE(mem_pub, pub);
    free(pub);
}

