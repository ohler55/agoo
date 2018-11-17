// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_READY_H
#define AGOO_READY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "err.h"

typedef enum {
    AGOO_READY_NONE	= '\0',
    AGOO_READY_IN	= 'i',
    AGOO_READY_OUT	= 'o',
    AGOO_READY_BOTH	= 'b',
} agooReadyIO;

typedef struct _agooReady	*agooReady;

typedef struct _agooHandler {
    agooReadyIO	(*io)(void *ctx);
    // return false to remove connection
    bool	(*check)(void *ctx, double now);
    bool	(*read)(agooReady ready, void *ctx);
    bool	(*write)(void *ctx);
    void	(*error)(void *ctx);
    void	(*destroy)(void *ctx);
} *agooHandler;

extern agooReady	agoo_ready_create(agooErr err);
extern void		agoo_ready_destroy(agooReady ready);
extern int		agoo_ready_add(agooErr		err,
				       agooReady	ready,
				       int		fd,
				       agooHandler	handler,
				       void		*ctx);
extern int		agoo_ready_go(agooErr err, agooReady ready);
extern void		agoo_ready_iterate(agooReady ready, void (*cb)(void *ctx, void *arg), void *arg);

#endif // AGOO_READY_H
