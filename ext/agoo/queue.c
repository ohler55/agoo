// Copyright 2015, 2016, 2018 by Peter Ohler, All Rights Reserved

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "debug.h"
#include "dtime.h"
#include "queue.h"

// lower gives faster response but burns more CPU. This is a reasonable compromise.
#define RETRY_SECS	0.0001
#define WAIT_MSECS	100

#define NOT_WAITING	0
#define WAITING		1
#define NOTIFIED	2

// head and tail both increment and wrap.
// tail points to next open space.
// When head == tail the queue is full. This happens when tail catches up with head.
// 

void
agoo_queue_init(agooQueue q, size_t qsize) {
    agoo_queue_multi_init(q, qsize, false, false);
}

void
agoo_queue_multi_init(agooQueue q, size_t qsize, bool multi_push, bool multi_pop) {
    if (qsize < 4) {
	qsize = 4;
    }
    q->q = (agooQItem*)malloc(sizeof(agooQItem) * qsize);
    DEBUG_ALLOC(mem_qitem, q->q)
    q->end = q->q + qsize;

    memset(q->q, 0, sizeof(agooQItem) * qsize);
    atomic_init(&q->head, q->q);
    atomic_init(&q->tail, q->q + 1);
    agoo_atomic_flag_init(&q->push_lock);
    agoo_atomic_flag_init(&q->pop_lock);
    atomic_init(&q->wait_state, 0);
    q->multi_push = multi_push;
    q->multi_pop = multi_pop;
    // Create when/if needed.
    q->rsock = 0;
    q->wsock = 0;
}

void
agoo_queue_cleanup(agooQueue q) {
    DEBUG_FREE(mem_qitem, q->q)
    free(q->q);
    q->q = NULL;
    q->end = NULL;
    if (0 < q->wsock) {
	close(q->wsock);
    }
    if (0 < q->rsock) {
	close(q->rsock);
    }
}

void
agoo_queue_push(agooQueue q, agooQItem item) {
    agooQItem	*tail;

    if (q->multi_push) {
	while (atomic_flag_test_and_set(&q->push_lock)) {
	    dsleep(RETRY_SECS);
	}
    }
    // Wait for head to move on.
    while (atomic_load(&q->head) == atomic_load(&q->tail)) {
	dsleep(RETRY_SECS);
    }
    *(agooQItem*)atomic_load(&q->tail) = item;
    tail = (agooQItem*)atomic_load(&q->tail) + 1;

    if (q->end <= tail) {
	tail = q->q;
    }
    atomic_store(&q->tail, tail);
    if (q->multi_push) {
	atomic_flag_clear(&q->push_lock);
    }
    if (0 != q->wsock && WAITING == (long)atomic_load(&q->wait_state)) {
	if (write(q->wsock, ".", 1)) {}
	atomic_store(&q->wait_state, NOTIFIED);
    }
}

void
agoo_queue_wakeup(agooQueue q) {
    if (0 != q->wsock) {
	if (write(q->wsock, ".", 1)) {}
    }
}

agooQItem
agoo_queue_pop(agooQueue q, double timeout) {
    agooQItem	item;
    agooQItem	*next;
    
    if (q->multi_pop) {
	while (atomic_flag_test_and_set(&q->pop_lock)) {
	    dsleep(RETRY_SECS);
	}
    }
    item = *(agooQItem*)atomic_load(&q->head);

    if (NULL != item) {
	*(agooQItem*)atomic_load(&q->head) = NULL;
	if (q->multi_pop) {
	    atomic_flag_clear(&q->pop_lock);
	}
	return item;
    }
    next = (agooQItem*)atomic_load(&q->head) + 1;

    if (q->end <= next) {
	next = q->q;
    }
    // If the next is the tail then wait for something to be appended.
    for (int cnt = (int)(timeout / (double)WAIT_MSECS * 1000.0); atomic_load(&q->tail) == next; cnt--) {
	struct pollfd	pa;

	if (cnt <= 0) {
	    if (q->multi_pop) {
		atomic_flag_clear(&q->pop_lock);
	    }
	    return NULL;
	}
	pa.fd = agoo_queue_listen(q);
	pa.events = POLLIN;
	pa.revents = 0;
	if (0 < poll(&pa, 1, WAIT_MSECS)) {
	    agoo_queue_release(q);
	}
    }
    atomic_store(&q->head, next);
    item = *next;
    *next = NULL;
    if (q->multi_pop) {
	atomic_flag_clear(&q->pop_lock);
    }
    return item;
}

// Called by the popper usually.
bool
agoo_queue_empty(agooQueue q) {
    agooQItem	*head = atomic_load(&q->head);
    agooQItem	*next = head + 1;

    if (q->end <= next) {
	next = q->q;
    }
    if (NULL == *head && atomic_load(&q->tail) == next) {
	return true;
    }
    return false;
}

int
agoo_queue_listen(agooQueue q) {
    if (0 == q->rsock) {
	int	fd[2];

	if (0 == pipe(fd)) {
	    fcntl(fd[0], F_SETFL, O_NONBLOCK);
	    fcntl(fd[1], F_SETFL, O_NONBLOCK);
	    q->rsock = fd[0];
	    q->wsock = fd[1];
	}
    }
    atomic_store(&q->wait_state, WAITING);
    
    return q->rsock;
}

void
agoo_queue_release(agooQueue q) {
    char	buf[8];

    // clear pipe
    while (0 < read(q->rsock, buf, sizeof(buf))) {
    }
    atomic_store(&q->wait_state, NOT_WAITING);
}

int
agoo_queue_count(agooQueue q) {
    int	size = (int)(q->end - q->q);
    
    return ((agooQItem*)atomic_load(&q->tail) - (agooQItem*)atomic_load(&q->head) + size) % size;
}

