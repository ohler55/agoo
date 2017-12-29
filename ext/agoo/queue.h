// Copyright 2015, 2016, 2018 by Peter Ohler, All Rights Reserved

#ifndef __AGOO_QUEUE_H__
#define __AGOO_QUEUE_H__

#include <stdatomic.h>
#include <stdbool.h>

typedef void	*QItem;

typedef struct _Queue {
    QItem		*q;
    QItem		*end;
    _Atomic(QItem*)	head;
    _Atomic(QItem*)	tail;
    bool		multi_push;
    bool		multi_pop;
    atomic_flag		push_lock; // set to true when push in progress
    atomic_flag		pop_lock; // set to true when push in progress
    atomic_int		wait_state;
    int			rsock;
    int			wsock;
} *Queue;

extern void	queue_init(Queue q, size_t qsize);

extern void	queue_multi_init(Queue q, size_t qsize, bool multi_push, bool multi_pop);

extern void	queue_cleanup(Queue q);
extern void	queue_push(Queue q, QItem item);
extern QItem	queue_pop(Queue q, double timeout);
extern bool	queue_empty(Queue q);
extern int	queue_listen(Queue q);
extern void	queue_release(Queue q);
extern int	queue_count(Queue q);

extern void	queue_wakeup(Queue q);

#endif /* __AGOO_QUEUE_H__ */
