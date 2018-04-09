// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdlib.h>
#include <string.h>

#include "pub.h"

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
pub_publish(char *subject, uint8_t *message, size_t mlen, bool bin) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub)) + - 8 + mlen + 1;

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_MSG;
	p->cid = 0;
	p->bin = bin;
	p->mlen = mlen;
	p->subject = strdup(subject);
	memcpy(p->msg, message, mlen);
	p->msg[mlen] = '\0'; // for strings and debugging
    }
    return p;
}

Pub
pub_write(uint64_t cid, uint8_t *message, size_t mlen, bool bin) {
    Pub	p = (Pub)malloc(sizeof(struct _Pub)) + - 8 + mlen + 1;

    if (NULL != p) {
	p->next = NULL;
	p->kind = PUB_WRITE;
	p->cid = cid;
	p->subject = NULL;
	p->bin = bin;
	p->mlen = mlen;
	memcpy(p->msg, message, mlen);
	p->msg[mlen] = '\0'; // for strings and debugging
    }
    return p;
}
