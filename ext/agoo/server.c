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

#define LOOP_UP		100

struct _agooServer	agoo_server = {false};

void
agoo_server_setup() {
    long	i;
    
    memset(&agoo_server, 0, sizeof(struct _agooServer));
    pthread_mutex_init(&agoo_server.up_lock, 0);
    agoo_server.up_list = NULL;
    agoo_server.max_push_pending = 32;
    agoo_pages_init();
    agoo_queue_multi_init(&agoo_server.con_queue, 1024, false, true);
    agoo_queue_multi_init(&agoo_server.eval_queue, 1024, true, true);
    agoo_server.loop_max = 4;
    if (0 < (i = sysconf(_SC_NPROCESSORS_ONLN))) {
	i /= 2;
	if (1 >= i) {
	    i = 1;
	}
	agoo_server.loop_max = (int)i;
    }
}

static void
add_con_loop() {
    struct _agooErr	err = AGOO_ERR_INIT;
    agooConLoop		loop = agoo_conloop_create(&err, 0);

    if (NULL != loop) {
	loop->next = agoo_server.con_loops;
	agoo_server.con_loops = loop;
	agoo_server.loop_cnt++;
    }
}

static void*
listen_loop(void *x) {
    int			optval = 1;
    struct pollfd	pa[100];
    struct pollfd	*p;
    struct _agooErr	err = AGOO_ERR_INIT;
    struct sockaddr_in	client_addr;
    int			client_sock;
    int			pcnt = 0;
    socklen_t		alen = 0;
    agooCon		con;
    int			i;
    uint64_t		cnt = 0;
    agooBind		b;

    // TBD support multiple sockets, count binds, allocate pollfd, setup
    //
    for (b = agoo_server.binds, p = pa; NULL != b; b = b->next, p++, pcnt++) {
	p->fd = b->fd;
	p->events = POLLIN;
	p->revents = 0;
    }
    memset(&client_addr, 0, sizeof(client_addr));
    atomic_fetch_add(&agoo_server.running, 1);
    while (agoo_server.active) {
	if (0 > (i = poll(pa, pcnt, 200))) {
	    if (EAGAIN == errno) {
		continue;
	    }
	    agoo_log_cat(&agoo_error_cat, "Server polling error. %s.", strerror(errno));
	    // Either a signal or something bad like out of memory. Might as well exit.
	    break;
	}
	if (0 == i) { // nothing to read
	    continue;
	}
	for (b = agoo_server.binds, p = pa; NULL != b; b = b->next, p++) {
	    if (0 != (p->revents & POLLIN)) {
		if (0 > (client_sock = accept(p->fd, (struct sockaddr*)&client_addr, &alen))) {
		    agoo_log_cat(&agoo_error_cat, "Server with pid %d accept connection failed. %s.", getpid(), strerror(errno));
		} else if (NULL == (con = agoo_con_create(&err, client_sock, ++cnt, b))) {
		    agoo_log_cat(&agoo_error_cat, "Server with pid %d accept connection failed. %s.", getpid(), err.msg);
		    close(client_sock);
		    cnt--;
		    agoo_err_clear(&err);
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
		    agoo_log_cat(&agoo_con_cat, "Server with pid %d accepted connection %llu on %s [%d]",
				 getpid(), (unsigned long long)cnt, b->id, con->sock);

		    con_cnt = atomic_fetch_add(&agoo_server.con_cnt, 1);
		    if (agoo_server.loop_max > agoo_server.loop_cnt && agoo_server.loop_cnt * LOOP_UP < con_cnt) {
			add_con_loop();
		    }
		    agoo_queue_push(&agoo_server.con_queue, (void*)con);
		}
	    }
	    if (0 != (p->revents & (POLLERR | POLLHUP | POLLNVAL))) {
		if (0 != (p->revents & (POLLHUP | POLLNVAL))) {
		    agoo_log_cat(&agoo_error_cat, "Agoo server with pid %d socket on %s closed.", getpid(), b->id);
		} else {
		    agoo_log_cat(&agoo_error_cat, "Agoo server with pid %d socket on %s error.", getpid(), b->id);
		}
		agoo_server.active = false;
	    }
	    p->revents = 0;
	}
    }
    for (b = agoo_server.binds; NULL != b; b = b->next) {
	agoo_bind_close(b);
    }
    atomic_fetch_sub(&agoo_server.running, 1);

    return NULL;
}

int
agoo_server_start(agooErr err, const char *app_name, const char *version) {
    double	giveup;
    int		xcnt = 0;
    
    pthread_create(&agoo_server.listen_thread, NULL, listen_loop, NULL);
    xcnt++;
    agoo_server.con_loops = agoo_conloop_create(err, 0);
    agoo_server.loop_cnt = 1;
    xcnt++;
    
    // If the eval thread count is 1 that implies the eval load is low so
    // might as well create the maximum number of con threads as is
    // reasonable.
    if (1 >= agoo_server.thread_cnt) {
	while (agoo_server.loop_cnt < agoo_server.loop_max) {
	    add_con_loop();
	    xcnt++;
	}
    }
    giveup = dtime() + 1.0;
    while (dtime() < giveup) {
	if (xcnt <= (long)atomic_load(&agoo_server.running)) {
	    break;
	}
	dsleep(0.01);
    }
    if (agoo_info_cat.on) {
	agooBind	b;
	
	for (b = agoo_server.binds; NULL != b; b = b->next) {
	    agoo_log_cat(&agoo_info_cat, "%s %s with pid %d is listening on %s.", app_name, version, getpid(), b->id);
	}
    }
    return AGOO_ERR_OK;
}

int
setup_listen(agooErr err) {
    agooBind	b;

    for (b = agoo_server.binds; NULL != b; b = b->next) {
	if (AGOO_ERR_OK != agoo_bind_listen(err, b)) {
	    return err->code;
	}
    }
    agoo_server.active = true;

    return AGOO_ERR_OK;
}

void
agoo_server_shutdown(const char *app_name, void (*stop)()) {
    if (agoo_server.inited) {
	agooConLoop	loop;

	agoo_log_cat(&agoo_info_cat, "%s with pid %d shutting down.", app_name, getpid());
	agoo_server.inited = false;
	if (agoo_server.active) {
	    double	giveup = dtime() + 1.0;
	    
	    agoo_server.active = false;
	    pthread_detach(agoo_server.listen_thread);
	    for (loop = agoo_server.con_loops; NULL != loop; loop = loop->next) {
		pthread_detach(loop->thread);
	    }
	    while (0 < (long)atomic_load(&agoo_server.running)) {
		dsleep(0.1);
		if (giveup < dtime()) {
		    break;
		}
	    }
	    if (NULL != stop) {
		stop();
	    }
	    while (NULL != agoo_server.hooks) {
		agooHook	h = agoo_server.hooks;

		agoo_server.hooks = h->next;
		agoo_hook_destroy(h);
	    }
	}
	while (NULL != agoo_server.binds) {
	    agooBind	b = agoo_server.binds;

	    agoo_server.binds = b->next;
	    agoo_bind_destroy(b);
	}
	agoo_queue_cleanup(&agoo_server.con_queue);
	while (NULL != (loop = agoo_server.con_loops)) {
	    agoo_server.con_loops = loop->next;
	    agoo_conloop_destroy(loop);
	}
	agoo_queue_cleanup(&agoo_server.eval_queue);

	agoo_pages_cleanup();
	agoo_http_cleanup();
    }
}

void
agoo_server_bind(agooBind b) {
    // If a bind with the same port already exists, replace it.
    agooBind	prev = NULL;

    if (NULL == b->read) {
	b->read = agoo_con_http_read;
    }
    if (NULL == b->write) {
	b->write = agoo_con_http_write;
    }
    if (NULL == b->events) {
	b->events = agoo_con_http_events;
    }
    for (agooBind bx = agoo_server.binds; NULL != bx; bx = bx->next) {
	if (bx->port == b->port) {
	    b->next = bx->next;
	    if (NULL == prev) {
		agoo_server.binds = b;
	    } else {
		prev->next = b;
	    }
	    agoo_bind_destroy(bx);
	    return;
	}
	prev = bx;
    }
    b->next = agoo_server.binds;
    agoo_server.binds = b;
}

void
agoo_server_add_upgraded(agooUpgraded up) {
    pthread_mutex_lock(&agoo_server.up_lock);
    if (NULL == agoo_server.up_list) {
	up->next = NULL;
    } else {
	agoo_server.up_list->prev = up;
    }
    up->next = agoo_server.up_list;
    agoo_server.up_list = up;
    up->con->up = up;
    pthread_mutex_unlock(&agoo_server.up_lock);
}

int
agoo_server_add_func_hook(agooErr	err,
			  agooMethod	method,
			  const char	*pattern,
			  void		(*func)(agooReq req),
			  agooQueue	queue,
			  bool		quick) {
    agooHook	h;
    agooHook	prev = NULL;
    agooHook	hook = agoo_hook_func_create(method, pattern, func, queue);

    if (NULL == hook) {
	return agoo_err_set(err, AGOO_ERR_MEMORY, "failed to allocate memory for HTTP server Hook.");
    }
    hook->no_queue = quick;
    for (h = agoo_server.hooks; NULL != h; h = h->next) {
	prev = h;
    }
    if (NULL != prev) {
	prev->next = hook;
    } else {
	agoo_server.hooks = hook;
    }
    return AGOO_ERR_OK;
}

void
agoo_server_publish(struct _agooPub *pub) {
    agooConLoop	loop;

    for (loop = agoo_server.con_loops; NULL != loop; loop = loop->next) {
	if (NULL == loop->next) {
	    agoo_queue_push(&loop->pub_queue, pub);
	} else {
	    agoo_queue_push(&loop->pub_queue, agoo_pub_dup(pub));
	}
    }
}
