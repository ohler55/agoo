// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_ATOMIC_H
#define AGOO_ATOMIC_H

#if HAVE_STDATOMIC_H

#include <stdatomic.h>

#define	AGOO_ATOMIC_INT_INIT(v)	(v)

static inline void
agoo_atomic_flag_init(atomic_flag *flagp) {
    atomic_flag_clear(flagp);
}

#else

// This is a poor attempt to implement the stdatomic calls needed for this
// project. No attempt was made to make it either fast or memory efficient. It
// works for older compilers and it turns out the overall impact on
// performance is small.

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

typedef struct _agooAtom {
    volatile void	*value;
    pthread_mutex_t	lock;
} *agooAtom;

#define	_Atomic(T) struct _agooAtom

#define	AGOO_ATOMIC_INT_INIT(v)	{ .value = (v), .lock = PTHREAD_MUTEX_INITIALIZER }
#define	ATOMIC_FLAG_INIT	{ .value = NULL, .lock = PTHREAD_MUTEX_INITIALIZER }

typedef struct _agooAtom	atomic_flag;
typedef struct _agooAtom	atomic_int;

static inline void
agoo_atomic_flag_init(atomic_flag *flagp) {
    flagp->value = NULL;
    pthread_mutex_init(&flagp->lock, NULL);
}

static inline void
atomic_init(agooAtom a, void *value) {
    a->value = value;
    pthread_mutex_init(&a->lock, NULL);
}

#define atomic_store(a, v) _atomic_store((a), (void*)(v))

static inline void
_atomic_store(agooAtom a, void *value) {
    pthread_mutex_lock(&a->lock);
    a->value = value;
    pthread_mutex_unlock(&a->lock);
}

static inline void*
atomic_load(agooAtom a) {
    void	*value;
    
    pthread_mutex_lock(&a->lock);
    value = (void*)a->value;
    pthread_mutex_unlock(&a->lock);

    return value;
}

static inline int
atomic_fetch_add(agooAtom a, int delta) {
    int	before;
    
    pthread_mutex_lock(&a->lock);
    before = (int)(long)a->value;
    a->value = (void*)(long)(before + delta);
    pthread_mutex_unlock(&a->lock);

    return before;
}

static inline int
atomic_fetch_sub(agooAtom a, int delta) {
    int	before;
    
    pthread_mutex_lock(&a->lock);
    before = (int)(long)a->value;
    a->value = (void*)(long)(before - delta);
    pthread_mutex_unlock(&a->lock);

    return before;
}

static inline void
atomic_flag_clear(agooAtom a) {
    pthread_mutex_lock(&a->lock);
    a->value = NULL;
    pthread_mutex_unlock(&a->lock);
}

static inline bool
atomic_flag_test_and_set(agooAtom a) {
    volatile void	*prev;
    
    pthread_mutex_lock(&a->lock);
    if (NULL == (prev = a->value)) {
	a->value = (void*)"true";
    }
    pthread_mutex_unlock(&a->lock);

    return (NULL != prev);
}

#endif

#endif // AGOO_ATOMIC_H
