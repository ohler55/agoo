// Copyright 2015, 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef AGOO_QUEUE_H
#define AGOO_QUEUE_H

#include <stdbool.h>
#include <stdlib.h>

#include "atomic.h"

typedef void	*agooQItem;

typedef struct _agooQueue {
    agooQItem		*q;
    agooQItem		*end;
    _Atomic(agooQItem*)	head;
    _Atomic(agooQItem*)	tail;
    bool		multi_push;
    bool		multi_pop;
    atomic_flag		push_lock; // set to true when push in progress
    atomic_flag		pop_lock; // set to true when push in progress
    atomic_int		wait_state;
    int			rsock;
    int			wsock;
} *agooQueue;

extern void		agoo_queue_init(agooQueue q, size_t qsize);

extern void		agoo_queue_multi_init(agooQueue q, size_t qsize, bool multi_push, bool multi_pop);

extern void		agoo_queue_cleanup(agooQueue q);
extern void		agoo_queue_push(agooQueue q, agooQItem item);
extern agooQItem	agoo_queue_pop(agooQueue q, double timeout);
extern bool		agoo_queue_empty(agooQueue q);
extern int		agoo_queue_listen(agooQueue q);
extern void		agoo_queue_release(agooQueue q);
extern int		agoo_queue_count(agooQueue q);

extern void		agoo_queue_wakeup(agooQueue q);

#endif // AGOO_QUEUE_H
