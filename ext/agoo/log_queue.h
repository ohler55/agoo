// Copyright 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_LOG_QUEUE_H
#define AGOO_LOG_QUEUE_H

#include <stdbool.h>

#include "atomic.h"
#include "val.h"

typedef struct _LogQueue {
    opoMsg		*q;
    opoMsg		*end;
    _Atomic(opoMsg*)	head;
    _Atomic(opoMsg*)	tail;
    atomic_flag		pop_lock; // set to true when ppo in progress
    atomic_int		wait_state;
    int			rsock;
    int			wsock;
} *LogQueue;

extern void	queue_init(Queue q, size_t qsize);

extern void	queue_cleanup(Queue q);
extern void	queue_push(Queue q, opoMsg item);
extern opoMsg	queue_pop(Queue q, double timeout);
extern bool	queue_empty(Queue q);
extern int	queue_count(Queue q);

#endif /* AGOO_LOG_QUEUE_H */
