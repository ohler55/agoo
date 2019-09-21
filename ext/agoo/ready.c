// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#else
#include <ctype.h>
#include <netdb.h>
#include <poll.h>
#endif

#include "debug.h"
#include "dtime.h"
#include "log.h"
#include "ready.h"

#define CHECK_FREQ		0.5
// milliseconds
#define MAX_WAIT		10

#if HAVE_SYS_EPOLL_H
#define EPOLL_SIZE		100
#else
#define INITIAL_POLL_SIZE	1024
#endif

typedef struct _link {
    struct _link	*next;
    struct _link	*prev;
    int			fd;
    void		*ctx;
    agooHandler		handler;
#if HAVE_SYS_EPOLL_H
    uint32_t		events; // last events set
#else
    struct pollfd	*pp;
#endif
} *Link;

struct _agooReady {
    Link	links;
    int		lcnt;
    double	next_check;
#if HAVE_SYS_EPOLL_H
    int		epoll_fd;
#else
    struct pollfd	*pa;
    struct pollfd	*pend;
#endif
};

static Link
link_create(agooErr err, int fd, void *ctx, agooHandler handler) {
    // TBD use block allocator
    Link	link = (Link)AGOO_MALLOC(sizeof(struct _link));

    if (NULL == link) {
	AGOO_ERR_MEM(err, "Connection Link");
    } else {
	//DEBUG_ALLOC(mem_???, c);
	link->next = NULL;
	link->prev = NULL;
	link->fd = fd;
	link->ctx = ctx;
	link->handler = handler;
    }
    return link;
}

agooReady
agoo_ready_create(agooErr err) {
    agooReady	ready = (agooReady)AGOO_MALLOC(sizeof(struct _agooReady));

    if (NULL == ready) {
	AGOO_ERR_MEM(err, "Connection Manager");
    } else {
	//DEBUG_ALLOC(mem_???, c);
	ready->links = NULL;
	ready->lcnt = 0;
	ready->next_check = dtime() + CHECK_FREQ;
#if HAVE_SYS_EPOLL_H
	if (0 > (ready->epoll_fd = epoll_create(1))) {
	    agoo_err_no(err, "epoll create failed");
	    return NULL;
	}
#else
	{
	    size_t	size = sizeof(struct pollfd) * INITIAL_POLL_SIZE;

	    ready->pa = (struct pollfd*)AGOO_MALLOC(size);
	    ready->pend = ready->pa + INITIAL_POLL_SIZE;
	    memset(ready->pa, 0, size);
	}
#endif
    }
    return ready;
}

void
agoo_ready_destroy(agooReady ready) {
    Link	link;

    while (NULL != (link = ready->links)) {
	ready->links = link->next;
	if (NULL != link->handler->destroy) {
	    link->handler->destroy(link->ctx);
	}
	AGOO_FREE(link);
    }
#if HAVE_SYS_EPOLL_H
    close(ready->epoll_fd);
#else
    AGOO_FREE(ready->pa);
#endif
    AGOO_FREE(ready);
}

int
agoo_ready_add(agooErr		err,
	       agooReady	ready,
	       int		fd,
	       agooHandler	handler,
	       void		*ctx) {
    Link	link;

    if (NULL == (link = link_create(err, fd, ctx, handler))) {
	return err->code;
    }
    link->next = ready->links;
    if (NULL != ready->links) {
	ready->links->prev = link;
    }
    ready->links = link;
    ready->lcnt++;

#if HAVE_SYS_EPOLL_H
    link->events = EPOLLIN;
    {
	struct epoll_event	event = {
	    .events = link->events,
	    .data = {
		.ptr = link,
	    },
	};
	if (0 > epoll_ctl(ready->epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
	    agoo_err_no(err, "epoll add failed");
	    return err->code;
	}
    }
#else
    if (ready->pend - ready->pa <= ready->lcnt) {
	size_t	cnt = (ready->pend - ready->pa) * 2;
	size_t	size = cnt * sizeof(struct pollfd);

	if (NULL == (ready->pa = (struct pollfd*)AGOO_REALLOC(ready->pa, size))) {
	    AGOO_ERR_MEM(err, "Connection Pool");
	    agoo_log_cat(&agoo_error_cat, "Out of memory.");
	    agoo_log_close();
	    exit(EXIT_FAILURE);

	    return err->code;
	}
	ready->pend = ready->pa + cnt;
	memset(ready->pa, 0, size);
    }
#endif
    return AGOO_ERR_OK;
}

static void
ready_remove(agooReady ready, Link link) {
    if (NULL == link->prev) {
	ready->links = link->next;
    } else {
	link->prev->next = link->next;
    }
    if (NULL != link->next) {
	link->next->prev = link->prev;
    }
#if HAVE_SYS_EPOLL_H
    {
	struct epoll_event	event = {
	    .events = 0,
	    .data = {
		.ptr = NULL,
	    },
	};
	if (0 > epoll_ctl(ready->epoll_fd, EPOLL_CTL_DEL, link->fd, &event)) {
	    agoo_log_cat(&agoo_error_cat, "epoll delete failed. %s", strerror(errno));
	}
    }
#endif
    if (NULL != link->handler->destroy) {
	link->handler->destroy(link->ctx);
    }
    AGOO_FREE(link);
    ready->lcnt--;
}

static void
ready_check_remove(agooReady ready, Link link) {
    if (NULL == link->handler->check || link->handler->check(link->ctx, 0.0)) {
	ready_remove(ready, link);
    }
}

int
agoo_ready_go(agooErr err, agooReady ready) {
    double	now;
    Link	link;
    Link	next;

#if HAVE_SYS_EPOLL_H
    struct epoll_event	events[EPOLL_SIZE];
    struct epoll_event	*ep;
    int			cnt;

    for (link = ready->links; NULL != link; link = link->next) {
	struct epoll_event	event = {
	    .events = 0,
	    .data = {
		.ptr = link,
	    },
	};
	switch (link->handler->io(link->ctx)) {
	case AGOO_READY_IN:
	    event.events = EPOLLIN;
	    break;
	case AGOO_READY_OUT:
	    event.events = EPOLLOUT;
	    break;
	case AGOO_READY_BOTH:
	    event.events = EPOLLIN | EPOLLOUT;
	    break;
	case AGOO_READY_NONE:
	default:
	    // ignore, either dead or closing
	    break;
	}
	if (event.events != link->events) {
	    if (0 > epoll_ctl(ready->epoll_fd, EPOLL_CTL_MOD, link->fd, &event)) {
		agoo_err_no(err, "epoll modifiy failed");
	    }
	    link->events = event.events;
	}
    }
    if (0 > (cnt = epoll_wait(ready->epoll_fd, events, sizeof(events) / sizeof(*events), MAX_WAIT))) {
	agoo_err_no(err, "Polling error.");
	agoo_log_cat(&agoo_error_cat, "%s", err->msg);
	return err->code;
    }
    for (ep = events; 0 < cnt; ep++, cnt--) {
	link = (Link)ep->data.ptr;
	if (0 != (ep->events & EPOLLIN) && NULL != link->handler->read) {
	    if (!link->handler->read(ready, link->ctx)) {
		ready_check_remove(ready, link);
		continue;
	    }
	}
	if (0 != (ep->events & EPOLLOUT && NULL != link->handler->write)) {
	    if (!link->handler->write(link->ctx)) {
		ready_check_remove(ready, link);
		continue;
	    }
	}
	if (0 != (ep->events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLPRI))) {
	    if (NULL != link->handler->error) {
		link->handler->error(link->ctx);
	    }
	    ready_check_remove(ready, link);
	    continue;
	}
    }
#else
    struct pollfd	*pp;
    int			i;

    // Setup the poll events.
    for (link = ready->links, pp = ready->pa; NULL != link; link = link->next, pp++) {
	pp->fd = link->fd;
	pp->revents = 0;
	link->pp = pp;
	switch (link->handler->io(link->ctx)) {
	case AGOO_READY_IN:
	    pp->events = POLLIN;
	    break;
	case AGOO_READY_OUT:
	    pp->events = POLLOUT;
	    break;
	case AGOO_READY_BOTH:
	    pp->events = POLLIN | POLLOUT;
	    break;
	case AGOO_READY_NONE:
	default:
	    // ignore, either dead or closing
	    link->pp = NULL;
	    pp--;
	    break;
	}
    }
    if (0 > (i = poll(ready->pa, (nfds_t)(pp - ready->pa), MAX_WAIT))) {
	if (EAGAIN == errno) {
	    return AGOO_ERR_OK;
	}
	agoo_err_no(err, "Polling error.");
	agoo_log_cat(&agoo_error_cat, "%s", err->msg);
	return err->code;
    }
    if (0 < i) {
	for (link = ready->links; NULL != link; link = next) {
	    next = link->next;
	    if (NULL == link->pp) {
		continue;
	    }
	    pp = link->pp;
	    if (0 != (pp->revents & POLLIN) && NULL != link->handler->read) {
		if (!link->handler->read(ready, link->ctx)) {
		    ready_check_remove(ready, link);
		    continue;
		}
	    }
	    if (0 != (pp->revents & POLLOUT && NULL != link->handler->write)) {
		if (!link->handler->write(link->ctx)) {
		    ready_check_remove(ready, link);
		    continue;
		}
	    }
	    if (0 != (pp->revents & (POLLERR | POLLHUP | POLLNVAL))) {
		if (NULL != link->handler->error) {
		    link->handler->error(link->ctx);
		}
		ready_check_remove(ready, link);
		continue;
	    }
	}
    }
#endif
    // Periodically check the connections to see if they are dead or not.
    now = dtime();
    if (ready->next_check <= now) {
	for (link = ready->links; NULL != link; link = next) {
	    next = link->next;
	    if (NULL != link->handler->check && link->handler->check(link->ctx, now)) {
		ready_remove(ready, link);
	    }
	}
	ready->next_check = dtime() + CHECK_FREQ;
    }
    return AGOO_ERR_OK;
}

void
agoo_ready_iterate(agooReady ready, void (*cb)(void *ctx, void *arg), void *arg) {
    Link	link;

    for (link = ready->links; NULL != link; link = link->next) {
	cb(link->ctx, arg);
    }
}
