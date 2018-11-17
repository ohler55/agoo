// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <string.h>

#include "con.h"
#include "debug.h"
#include "pub.h"
#include "server.h"
#include "subject.h"
#include "upgraded.h"

static void
destroy(agooUpgraded up) {
    agooSubject	subject;

    if (NULL != up->on_destroy) {
	up->on_destroy(up);
    }
    if (NULL == up->prev) {
	the_server.up_list = up->next;
	if (NULL != up->next) {
	    up->next->prev = NULL;
	}
    } else {
	up->prev->next = up->next;
	if (NULL != up->next) {
	    up->next->prev = up->prev;
	}
    }
    while (NULL != (subject = up->subjects)) {
	up->subjects = up->subjects->next;
	subject_destroy(subject);
    }
    DEBUG_FREE(mem_upgraded, up);
    free(up);
}

void
upgraded_release(agooUpgraded up) {
    pthread_mutex_lock(&the_server.up_lock);
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	destroy(up);
    }    
    pthread_mutex_unlock(&the_server.up_lock);
}

void
upgraded_release_con(agooUpgraded up) {
    pthread_mutex_lock(&the_server.up_lock);
    up->con = NULL;
    if (atomic_fetch_sub(&up->ref_cnt, 1) <= 1) {
	destroy(up);
    }
    pthread_mutex_unlock(&the_server.up_lock);
}

// Called from the con_loop thread, no need to lock, this steals the subject
// so the pub subject should set to NULL
void
upgraded_add_subject(agooUpgraded up, agooSubject subject) {
    agooSubject	s;

    for (s = up->subjects; NULL != s; s = s->next) {
	if (0 == strcmp(subject->pattern, s->pattern)) {
	    subject_destroy(subject);
	    return;
	}
    }
    subject->next = up->subjects;
    up->subjects = subject;
}

void
upgraded_del_subject(agooUpgraded up, agooSubject subject) {
    if (NULL == subject) {
	while (NULL != (subject = up->subjects)) {
	    up->subjects = up->subjects->next;
	    subject_destroy(subject);
	}
    } else {
	agooSubject	s;
	agooSubject	prev = NULL;

	for (s = up->subjects; NULL != s; s = s->next) {
	    if (0 == strcmp(subject->pattern, s->pattern)) {
		if (NULL == prev) {
		    up->subjects = s->next;
		} else {
		    prev->next = s->next;
		}
		subject_destroy(s);
		break;
	    }
	    prev = s;
	}
    }
}

bool
upgraded_match(agooUpgraded up, const char *subject) {
    agooSubject	s;

    for (s = up->subjects; NULL != s; s = s->next) {
	if (subject_check(s, subject)) {
	    return true;
	}
    }
    return false;
}

void
upgraded_ref(agooUpgraded up) {
    atomic_fetch_add(&up->ref_cnt, 1);
}

bool
upgraded_write(agooUpgraded up, const char *message, size_t mlen, bool bin, bool inc_ref) {
    agooPub	p;

    if (0 < the_server.max_push_pending && the_server.max_push_pending <= (long)atomic_load(&up->pending)) {
	atomic_fetch_sub(&up->ref_cnt, 1);
	// Too many pending messages.
	return false;
    }
    if (inc_ref) {
	atomic_fetch_add(&up->ref_cnt, 1);
    }
    p = pub_write(up, message, mlen, bin);
    atomic_fetch_add(&up->pending, 1);
    server_publish(p);

    return true;
}

void
upgraded_subscribe(agooUpgraded up, const char *subject, int slen, bool inc_ref) {
    if (inc_ref) {
	atomic_fetch_add(&up->ref_cnt, 1);
    }
    atomic_fetch_add(&up->pending, 1);
    server_publish(pub_subscribe(up, subject, slen));
}

void
upgraded_unsubscribe(agooUpgraded up, const char *subject, int slen, bool inc_ref) {
    if (inc_ref) {
	atomic_fetch_add(&up->ref_cnt, 1);
    }
    atomic_fetch_add(&up->pending, 1);
    server_publish(pub_unsubscribe(up, subject, slen));
}

void
upgraded_close(agooUpgraded up, bool inc_ref) {
    if (inc_ref) {
	atomic_fetch_add(&up->ref_cnt, 1);
    }
    atomic_fetch_add(&up->pending, 1);
    server_publish(pub_close(up));
}

int
upgraded_pending(agooUpgraded up) {
    return (int)(long)atomic_load(&up->pending);
}

agooUpgraded
upgraded_create(agooCon c, void * ctx, void *env) {
    agooUpgraded	up = (agooUpgraded)malloc(sizeof(struct _agooUpgraded));

    if (NULL != up) {
	DEBUG_ALLOC(mem_upgraded, up);
	memset(up, 0, sizeof(struct _agooUpgraded));
	up->con = c;
	up->ctx = ctx;
	up->env = env;
	atomic_init(&up->pending, 0);
	atomic_init(&up->ref_cnt, 0);
	atomic_store(&up->ref_cnt, 1); // start with 1 for the Con reference
    }
    return up;
}
