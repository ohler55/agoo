// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "con.h"
#include "dtime.h"
#include "http.h"
#include "hook.h"
#include "log.h"
#include "page.h"
#include "pub.h"
#include "upgraded.h"

#include "server.h"

#define LOOP_UP		512
#define MAX_LOOP	4

struct _Server	the_server = {false};

void
server_setup() {
    memset(&the_server, 0, sizeof(struct _Server));
    pthread_mutex_init(&the_server.up_lock, 0);
    the_server.up_list = NULL;
    the_server.max_push_pending = 32;
    pages_init();
    queue_multi_init(&the_server.con_queue, 1024, false, true);
    queue_multi_init(&the_server.eval_queue, 1024, true, true);
}

static void
add_con_loop() {
    struct _Err	err = ERR_INIT;
    ConLoop	loop = conloop_create(&err, 0);

    if (NULL != loop) {
	loop->next = the_server.con_loops;
	the_server.con_loops = loop;
	the_server.loop_cnt++;
    }
}

static void*
listen_loop(void *x) {
    int			optval = 1;
    struct pollfd	pa[100];
    struct pollfd	*p;
    struct _Err		err = ERR_INIT;
    struct sockaddr_in	client_addr;
    int			client_sock;
    int			pcnt = 0;
    socklen_t		alen = 0;
    Con			con;
    int			i;
    uint64_t		cnt = 0;
    Bind		b;

    // TBD support multiple sockets, count binds, allocate pollfd, setup
    //
    for (b = the_server.binds, p = pa; NULL != b; b = b->next, p++, pcnt++) {
	p->fd = b->fd;
	p->events = POLLIN;
	p->revents = 0;
    }
    memset(&client_addr, 0, sizeof(client_addr));
    atomic_fetch_add(&the_server.running, 1);
    while (the_server.active) {
	if (0 > (i = poll(pa, pcnt, 200))) {
	    if (EAGAIN == errno) {
		continue;
	    }
	    log_cat(&error_cat, "Server polling error. %s.", strerror(errno));
	    // Either a signal or something bad like out of memory. Might as well exit.
	    break;
	}
	if (0 == i) { // nothing to read
	    continue;
	}
	for (b = the_server.binds, p = pa; NULL != b; b = b->next, p++) {
	    if (0 != (p->revents & POLLIN)) {
		if (0 > (client_sock = accept(p->fd, (struct sockaddr*)&client_addr, &alen))) {
		    log_cat(&error_cat, "Server with pid %d accept connection failed. %s.", getpid(), strerror(errno));
		} else if (NULL == (con = con_create(&err, client_sock, ++cnt, b))) {
		    log_cat(&error_cat, "Server with pid %d accept connection failed. %s.", getpid(), err.msg);
		    close(client_sock);
		    cnt--;
		    err_clear(&err);
		} else {
		    int	con_cnt;
#ifdef OSX_OS
		    setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif
#ifdef PLATFORM_LINUX
		    setsockopt(client_sock, IPPROTO_TCP, TCP_QUICKACK, &optval, sizeof(optval));
#endif
		    fcntl(client_sock, F_SETFL, O_NONBLOCK);
		    //fcntl(client_sock, F_SETFL, FNDELAY);
		    setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
		    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
		    log_cat(&con_cat, "Server with pid %d accepted connection %llu on %s [%d]",
			    getpid(), (unsigned long long)cnt, b->id, con->sock);

		    con_cnt = atomic_fetch_add(&the_server.con_cnt, 1);
		    if (MAX_LOOP > the_server.loop_cnt && the_server.loop_cnt * LOOP_UP < con_cnt) {
			add_con_loop();
		    }
		    queue_push(&the_server.con_queue, (void*)con);
		}
	    }
	    if (0 != (p->revents & (POLLERR | POLLHUP | POLLNVAL))) {
		if (0 != (p->revents & (POLLHUP | POLLNVAL))) {
		    log_cat(&error_cat, "Agoo server with pid %d socket on %s closed.", getpid(), b->id);
		} else {
		    log_cat(&error_cat, "Agoo server with pid %d socket on %s error.", getpid(), b->id);
		}
		the_server.active = false;
	    }
	    p->revents = 0;
	}
    }
    for (b = the_server.binds; NULL != b; b = b->next) {
	bind_close(b);
    }
    atomic_fetch_sub(&the_server.running, 1);

    return NULL;
}

int
server_start(Err err, const char *app_name, const char *version) {
    double	giveup;
    
    pthread_create(&the_server.listen_thread, NULL, listen_loop, NULL);
    the_server.con_loops = conloop_create(err, 0);
    the_server.loop_cnt = 1;

    giveup = dtime() + 1.0;
    while (dtime() < giveup) {
	if (2 <= atomic_load(&the_server.running)) {
	    break;
	}
	dsleep(0.01);
    }
    if (info_cat.on) {
	Bind	b;
	
	for (b = the_server.binds; NULL != b; b = b->next) {
	    log_cat(&info_cat, "%s %s with pid %d is listening on %s.", app_name, version, getpid(), b->id);
	}
    }
    return ERR_OK;
}

int
setup_listen(Err err) {
    Bind	b;

    for (b = the_server.binds; NULL != b; b = b->next) {
	if (ERR_OK != bind_listen(err, b)) {
	    return err->code;
	}
    }
    the_server.active = true;

    return ERR_OK;
}

void
server_shutdown(const char *app_name, void (*stop)()) {
    if (the_server.inited) {
	ConLoop	loop;

	log_cat(&info_cat, "%s with pid %d shutting down.", app_name, getpid());
	the_server.inited = false;
	if (the_server.active) {
	    double	giveup = dtime() + 1.0;
	    
	    the_server.active = false;
	    pthread_detach(the_server.listen_thread);
	    for (loop = the_server.con_loops; NULL != loop; loop = loop->next) {
		pthread_detach(loop->thread);
	    }
	    while (0 < atomic_load(&the_server.running)) {
		dsleep(0.1);
		if (giveup < dtime()) {
		    break;
		}
	    }
	    if (NULL != stop) {
		stop();
	    }
	    while (NULL != the_server.hooks) {
		Hook	h = the_server.hooks;

		the_server.hooks = h->next;
		hook_destroy(h);
	    }
	}
	while (NULL != the_server.binds) {
	    Bind	b = the_server.binds;

	    the_server.binds = b->next;
	    bind_destroy(b);
	}
	queue_cleanup(&the_server.con_queue);
	for (loop = the_server.con_loops; NULL != loop; loop = loop->next) {
	    queue_cleanup(&loop->pub_queue);
	}
	queue_cleanup(&the_server.eval_queue);

	pages_cleanup();
	http_cleanup();
    }
}

void
server_bind(Bind b) {
    // If a bind with the same port already exists, replace it.
    Bind	prev = NULL;

    if (NULL == b->read) {
	b->read = con_http_read;
    }
    if (NULL == b->write) {
	b->write = con_http_write;
    }
    if (NULL == b->events) {
	b->events = con_http_events;
    }
    for (Bind bx = the_server.binds; NULL != bx; bx = bx->next) {
	if (bx->port == b->port) {
	    b->next = bx->next;
	    if (NULL == prev) {
		the_server.binds = b;
	    } else {
		prev->next = b;
	    }
	    bind_destroy(bx);
	    return;
	}
	prev = bx;
    }
    b->next = the_server.binds;
    the_server.binds = b;
}

void
server_add_upgraded(Upgraded up) {
    pthread_mutex_lock(&the_server.up_lock);
    if (NULL == the_server.up_list) {
	up->next = NULL;
    } else {
	the_server.up_list->prev = up;
    }
    up->next = the_server.up_list;
    the_server.up_list = up;
    up->con->up = up;
    pthread_mutex_unlock(&the_server.up_lock);
}

int
server_add_func_hook(Err	err,
		     Method	method,
		     const char	*pattern,
		     void	(*func)(Req req),
		     Queue	queue,
		     bool	quick) {
    Hook	h;
    Hook	prev = NULL;
    Hook	hook = hook_func_create(method, pattern, func, queue);

    if (NULL == hook) {
	return err_set(err, ERR_MEMORY, "failed to allocate memory for HTTP server Hook.");
    }
    hook->no_queue = quick;
    for (h = the_server.hooks; NULL != h; h = h->next) {
	prev = h;
    }
    if (NULL != prev) {
	prev->next = hook;
    } else {
	the_server.hooks = hook;
    }
    return ERR_OK;
}

void
server_publish(struct _Pub *pub) {
    ConLoop	loop;

    for (loop = the_server.con_loops; NULL != loop; loop = loop->next) {
	if (NULL == loop->next) {
	    queue_push(&loop->pub_queue, pub);
	} else {
	    queue_push(&loop->pub_queue, pub_dup(pub));
	}
    }
}
