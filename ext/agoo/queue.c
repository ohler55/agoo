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
queue_init(Queue q, size_t qsize) {
    queue_multi_init(q, qsize, false, false);
}

void
queue_multi_init(Queue q, size_t qsize, bool multi_push, bool multi_pop) {
    if (qsize < 4) {
	qsize = 4;
    }
    q->q = (QItem*)malloc(sizeof(QItem) * qsize);
    DEBUG_ALLOC(mem_qitem, q->q)
    q->end = q->q + qsize;

    memset(q->q, 0, sizeof(QItem) * qsize);
    q->head = q->q;
    q->tail = q->q + 1;
    atomic_flag_clear(&q->push_lock);
    atomic_flag_clear(&q->pop_lock);
    q->wait_state = 0;
    q->multi_push = multi_push;
    q->multi_pop = multi_pop;
    // Create when/if needed.
    q->rsock = 0;
    q->wsock = 0;
}

void
queue_cleanup(Queue q) {
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
queue_push(Queue q, QItem item) {
    QItem	*tail;

    if (q->multi_push) {
	while (atomic_flag_test_and_set(&q->push_lock)) {
	    dsleep(RETRY_SECS);
	}
    }
    // Wait for head to move on.
    while (atomic_load(&q->head) == q->tail) {
	dsleep(RETRY_SECS);
    }
    *q->tail = item;
    tail = q->tail + 1;

    if (q->end <= tail) {
	tail = q->q;
    }
    atomic_store(&q->tail, tail);
    if (q->multi_push) {
	atomic_flag_clear(&q->push_lock);
    }
    if (0 != q->wsock && WAITING == atomic_load(&q->wait_state)) {
	if (write(q->wsock, ".", 1)) {}
	atomic_store(&q->wait_state, NOTIFIED);
    }
}

void
queue_wakeup(Queue q) {
    if (0 != q->wsock) {
	if (write(q->wsock, ".", 1)) {}
    }
}

QItem
queue_pop(Queue q, double timeout) {
    QItem	item;
    QItem	*next;
    
    if (q->multi_pop) {
	while (atomic_flag_test_and_set(&q->pop_lock)) {
	    dsleep(RETRY_SECS);
	}
    }
    item = *q->head;

    if (NULL != item) {
	*q->head = NULL;
	if (q->multi_pop) {
	    atomic_flag_clear(&q->pop_lock);
	}
	return item;
    }
    next = q->head + 1;

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
	pa.fd = queue_listen(q);
	pa.events = POLLIN;
	pa.revents = 0;
	if (0 < poll(&pa, 1, WAIT_MSECS)) {
	    queue_release(q);
	}
    }
    atomic_store(&q->head, next);

    item = *q->head;
    *q->head = NULL;
    if (q->multi_pop) {
	atomic_flag_clear(&q->pop_lock);
    }
    return item;
}

// Called by the popper usually.
bool
queue_empty(Queue q) {
    QItem	*head = atomic_load(&q->head);
    QItem	*next = head + 1;

    if (q->end <= next) {
	next = q->q;
    }
    if (NULL == *head && q->tail == next) {
	return true;
    }
    return false;
}

int
queue_listen(Queue q) {
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
queue_release(Queue q) {
    char	buf[8];

    // clear pipe
    while (0 < read(q->rsock, buf, sizeof(buf))) {
    }
    atomic_store(&q->wait_state, NOT_WAITING);
}

int
queue_count(Queue q) {
    int	size = (int)(q->end - q->q);
    
    return (q->tail - q->head + size) % size;
}

