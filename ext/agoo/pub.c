// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "pub.h"
#include "text.h"

Pub
pub_close(uint64_t cid) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_CLOSE;
	p->cid = cid;
	p->sid = 0;
	p->bin = false;
	p->subject = NULL;
    }
    return p;
}

Pub
pub_subscribe(uint64_t cid, uint64_t sid, const char *subject) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_SUB;
	p->cid = cid;
	p->sid = sid;
	p->bin = false;
	p->subject = strdup(subject);
    }
    return p;
}

Pub
pub_unsubscribe(uint64_t cid, uint64_t sid) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_UN;
	p->cid = cid;
	p->sid = sid;
	p->bin = false;
	p->subject = NULL;
    }
    return p;
}

Pub
pub_publish(char *subject, const char *message, size_t mlen, bool bin) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_MSG;
	p->cid = 0;
	p->bin = bin;
	p->subject = strdup(subject);
	// Allocate an extra 16 bytes so the message can be expanded in place
	// if a WebSocket write.
	p->msg = text_allocate(mlen + 16);
	p->msg = text_append(text_allocate(mlen + 16), message, mlen);
	text_ref(p->msg);
    }
    return p;
}

Pub
pub_write(uint64_t cid, const char *message, size_t mlen, bool bin) {
    // Allocate an extra 16 bytes so the message can be expanded in place if a
    // WebSocket write.
    Pub	p = (Pub)malloc(sizeof(struct _Pub));

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_WRITE;
	p->cid = cid;
	p->subject = NULL;
	p->bin = bin;
	// Allocate an extra 16 bytes so the message can be expanded in place
	// if a WebSocket write.
	p->msg = text_allocate(mlen + 16);
	p->msg = text_append(text_allocate(mlen + 16), message, mlen);
	text_ref(p->msg);
    }
    return p;
}
