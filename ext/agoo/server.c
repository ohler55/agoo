// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdatomic.h>

#include <ruby.h>
#include <ruby/thread.h>

#include "con.h"
#include "debug.h"
#include "dtime.h"
#include "err.h"
#include "http.h"
#include "response.h"
#include "request.h"
#include "server.h"
#include "sub.h"
#include "upgraded.h"
#include "websocket.h"

static VALUE	server_class = Qundef;

static VALUE	connect_sym;
static VALUE	delete_sym;
static VALUE	get_sym;
static VALUE	head_sym;
static VALUE	options_sym;
static VALUE	post_sym;
static VALUE	push_env_key;
static VALUE	put_sym;

static ID	call_id;
static ID	each_id;
static ID	on_request_id;
static ID	to_i_id;

static const char	err500[] = "HTTP/1.1 500 Internal Server Error\r\n";

Server	the_server = NULL;

static void
shutdown_server(Server server) {
    if (NULL != server && server->active) {
	the_server = NULL;

	log_cat(&server->info_cat, "Agoo shutting down.");
	server->active = false;
	if (0 != server->listen_thread) {
	    pthread_join(server->listen_thread, NULL);
	    server->listen_thread = 0;
	}
	if (0 != server->con_thread) {
	    pthread_join(server->con_thread, NULL);
	    server->con_thread = 0;
	}
	queue_cleanup(&server->con_queue);
	queue_cleanup(&server->pub_queue);
	sub_cleanup(&server->sub_cache);
	cc_cleanup(&server->con_cache);
	// The preferred method to of waiting for the ruby threads would be
	// either a join or even a kill but since we don't have the gvi here
	// that would cause a segfault. Instead we set a timeout and wait for
	// the running counter to drop to zero.
	if (NULL != server->eval_threads) {
	    double	timeout = dtime() + 2.0;

	    while (dtime() < timeout) {
		if (0 >= atomic_load(&server->running)) {
		    break;
		}
		dsleep(0.02);
	    }
	    DEBUG_FREE(mem_eval_threads)
	    xfree(server->eval_threads);
	    server->eval_threads = NULL;
	}
	while (NULL != server->hooks) {
	    Hook	h = server->hooks;

	    server->hooks = h->next;
	    hook_destroy(h);
	}
	queue_cleanup(&server->eval_queue);
	log_close(&server->log);
	debug_print_stats();    
    }
}

static void
sig_handler(int sig) {
    if (NULL != the_server) {
	shutdown_server(the_server);
    }
    // Use exit instead of rb_exit as rb_exit segfaults most of the time.
    //rb_exit(0);
    exit(0);
}

static void
server_free(void *ptr) {
    shutdown_server((Server)ptr);
    // Commented out for now as it causes a segfault later. Some thread seems
    // to be pointing at it even though they have exited so live with a memory
    // leak that only shows up when the program exits.
    //xfree(ptr);
    DEBUG_FREE(mem_server)
    the_server = NULL;
}

static int
configure(Err err, Server s, int port, const char *root, VALUE options) {
    s->port = port;
    s->root = strdup(root);
    s->thread_cnt = 0;
    s->running = 0;
    s->listen_thread = 0;
    s->con_thread = 0;
    s->max_push_pending = 32;
    s->log.cats = NULL;
    log_cat_reg(&s->log, &s->error_cat, "ERROR",    ERROR, RED, true);
    log_cat_reg(&s->log, &s->warn_cat,  "WARN",     WARN,  YELLOW, true);
    log_cat_reg(&s->log, &s->info_cat,  "INFO",     INFO,  GREEN, true);
    log_cat_reg(&s->log, &s->debug_cat, "DEBUG",    DEBUG, GRAY, false);
    log_cat_reg(&s->log, &s->con_cat,   "connect",  INFO,  GREEN, false);
    log_cat_reg(&s->log, &s->req_cat,   "request",  INFO,  CYAN, false);
    log_cat_reg(&s->log, &s->resp_cat,  "response", INFO,  DARK_CYAN, false);
    log_cat_reg(&s->log, &s->eval_cat,  "eval",     INFO,  BLUE, false);
    log_cat_reg(&s->log, &s->push_cat,  "push",     INFO,  DARK_CYAN, false);

    if (ERR_OK != log_init(err, &s->log, options)) {
	return err->code;
    }
    if (Qnil != options) {
	VALUE	v;

	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("thread_count"))))) {
	    int	tc = FIX2INT(v);

	    if (1 <= tc || tc < 1000) {
		s->thread_cnt = tc;
	    } else {
		rb_raise(rb_eArgError, "thread_count must be between 1 and 1000.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("max_push_pending"))))) {
	    int	tc = FIX2INT(v);

	    if (0 <= tc || tc < 1000) {
		s->thread_cnt = tc;
	    } else {
		rb_raise(rb_eArgError, "thread_count must be between 0 and 1000.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("pedantic"))))) {
	    s->pedantic = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("Port"))))) {
	    if (rb_cInteger == rb_obj_class(v)) {
		s->port = NUM2INT(v);
	    } else {
		switch (rb_type(v)) {
		case T_STRING:
		    s->port = atoi(StringValuePtr(v));
		    break;
		case T_FIXNUM:
		    s->port = NUM2INT(v);
		    break;
		default:
		    break;
		}
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("quiet"))))) {
	    if (Qtrue == v) {
		s->info_cat.on = false;
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("debug"))))) {
	    if (Qtrue == v) {
		s->error_cat.on = true;
		s->warn_cat.on = true;
		s->info_cat.on = true;
		s->debug_cat.on = true;
		s->con_cat.on = true;
		s->req_cat.on = true;
		s->resp_cat.on = true;
		s->eval_cat.on = true;
	    }
	}
    }
    return ERR_OK;
}

/* Document-method: new
 *
 * call-seq: new(port, root, options)
 *
 * Creates a new server that will listen on the designated _port_ and using
 * the _root_ as the root of the static resources. Logging is feature based
 * and not level based and the options reflect that approach.
 *
 * - *options* [_Hash_] server options
 *
 *   - *:pedantic* [_true_|_false_] if true response header and status codes are check and an exception raised if they violate the rack spec at https://github.com/rack/rack/blob/master/SPEC, https://tools.ietf.org/html/rfc3875#section-4.1.18, or https://tools.ietf.org/html/rfc7230.
 *
 *   - *:thread_count* [_Integer_] number of ruby worker threads. Defaults to one. If zero then the _start_ function will not return but instead will proess using the thread that called _start_. Usually the default is best unless the workers are making IO calls.
 *
 *   - *:log_dir* [_String_] directory to place log files in. If nil or empty then no log files are written.
 *
 *   - *:log_console* [_true_|_false_] if true log entry are display on the console.
 *
 *   - *:log_classic* [_true_|_false_] if true log entry follow a classic format. If false log entries are JSON.
 *
 *   - *:log_colorize* [_true_|_false_] if true log entries are colorized.
 *
 *   - *:log_states* [_Hash_] a map of logging categories and whether they should be on or off. Categories are:
 *     - *:ERROR* errors
 *     - *:WARN* warnings
 *     - *:INFO* infomational
 *     - *:DEBUG* debugging
 *     - *:connect* openning and closing of connections
 *     - *:request* requests
 *     - *:response* responses
 *     - *:eval* handler evaluationss
 */
static VALUE
server_new(int argc, VALUE *argv, VALUE self) {
    Server	s;
    struct _Err	err = ERR_INIT;
    int		port;
    const char	*root;
    VALUE	options = Qnil;
    
    if (argc < 2 || 3 < argc) {
	rb_raise(rb_eArgError, "Wrong number of arguments to Agoo::Server.new.");
    }
    port = FIX2INT(argv[0]);
    rb_check_type(argv[1], T_STRING);
    root = StringValuePtr(argv[1]);
    if (3 <= argc) {
	options = argv[2];
    }
    s = ALLOC(struct _Server);
    DEBUG_ALLOC(mem_server)
    memset(s, 0, sizeof(struct _Server));
    if (ERR_OK != configure(&err, s, port, root, options)) {
	xfree(s);
	DEBUG_FREE(mem_server)
	// TBD raise Agoo specific exception
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    queue_multi_init(&s->con_queue, 256, false, false);
    queue_multi_init(&s->pub_queue, 256, true, false);
    queue_multi_init(&s->eval_queue, 1024, false, true);

    cache_init(&s->pages);
    cc_init(&s->con_cache);
    sub_init(&s->sub_cache);

    the_server = s;
    
    return Data_Wrap_Struct(server_class, NULL, server_free, s);
}

static void*
listen_loop(void *x) {
    Server		server = (Server)x;
    struct sockaddr_in	addr;
    int			optval = 1;
    struct pollfd	pa[1];
    struct pollfd	*fp = pa;
    struct _Err		err = ERR_INIT;
    struct sockaddr_in	client_addr;
    int			client_sock;
    socklen_t		alen = 0;
    Con			con;
    int			i;
    uint64_t		cnt = 0;

    atomic_fetch_add(&server->running, 1);
    if (0 >= (pa->fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))) {
	log_cat(&server->error_cat, "Server failed to open server socket. %s.", strerror(errno));
	atomic_fetch_sub(&server->running, 1);
	return NULL;
    }
#ifdef OSX_OS 
    setsockopt(pa->fd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    setsockopt(pa->fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    setsockopt(pa->fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (0 > bind(fp->fd, (struct sockaddr*)&addr, sizeof(addr))) {
	log_cat(&server->error_cat, "Server failed to bind server socket. %s.", strerror(errno));
	atomic_fetch_sub(&server->running, 1);
	return NULL;
    }
    listen(pa->fd, 1000);
    server->ready = true;
    pa->events = POLLIN;
    pa->revents = 0;

    memset(&client_addr, 0, sizeof(client_addr));
    while (server->active) {
	if (0 > (i = poll(pa, 1, 100))) {
	    if (EAGAIN == errno) {
		continue;
	    }
	    log_cat(&server->error_cat, "Server polling error. %s.", strerror(errno));
	    // Either a signal or something bad like out of memory. Might as well exit.
	    break;
	}
	if (0 == i) { // nothing to read
	    continue;
	}
	if (0 != (pa->revents & POLLIN)) {
	    if (0 > (client_sock = accept(pa->fd, (struct sockaddr*)&client_addr, &alen))) {
		log_cat(&server->error_cat, "Server accept connection failed. %s.", strerror(errno));
	    } else if (NULL == (con = con_create(&err, server, client_sock, ++cnt))) {
		log_cat(&server->error_cat, "Server accept connection failed. %s.", err.msg);
		close(client_sock);
		cnt--;
		err_clear(&err);
	    } else {
#ifdef OSX_OS
		setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif
		fcntl(client_sock, F_SETFL, O_NONBLOCK);
		setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
		setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
		log_cat(&server->con_cat, "Server accepted connection %llu on port %d [%d]", (unsigned long long)cnt, server->port, con->sock);
		queue_push(&server->con_queue, (void*)con);
	    }
	}
	if (0 != (pa->revents & (POLLERR | POLLHUP | POLLNVAL))) {
	    if (0 != (pa->revents & (POLLHUP | POLLNVAL))) {
		log_cat(&server->error_cat, "Agoo server socket on port %d closed.", server->port);
	    } else {
		log_cat(&server->error_cat, "Agoo server socket on port %d error.", server->port);
	    }
	    server->active = false;
	}
	pa->revents = 0;
    }
    close(pa->fd);
    atomic_fetch_sub(&server->running, 1);

    return NULL;
}

static const char	bad500[] = "HTTP/1.1 500 Internal Error\r\nConnection: Close\r\nContent-Length: ";

static VALUE
rescue_error(VALUE x) {
    Req			req = (Req)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);
    char		buf[1024];
    int			len = (int)(strlen(classname) + 2 + strlen(ms));
    int			cnt;
    Text		message;

    if ((int)(sizeof(buf) - sizeof(bad500) + 7) <= len) {
	len = sizeof(buf) - sizeof(bad500) + 7;
    }
    cnt = snprintf(buf, sizeof(buf), "%s%d\r\n\r\n%s: %s", bad500, len, classname, ms);
    message = text_create(buf, cnt);

    req->res->close = true;
    res_set_message(req->res, message);
    queue_wakeup(&req->server->con_queue);

    return Qfalse;
}

static VALUE
handle_base_inner(void *x) {
    Req			req = (Req)x;
    volatile VALUE	rr = request_wrap(req);
    volatile VALUE	rres = response_new(req->server);
    
    rb_funcall(req->handler, on_request_id, 2, rr, rres);
    DATA_PTR(rr) = NULL;

    res_set_message(req->res, response_text(rres));
    queue_wakeup(&req->server->con_queue);

    return Qfalse;
}

static void*
handle_base(void *x) {
    rb_rescue2(handle_base_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);

    return NULL;
}

static int
header_cb(VALUE key, VALUE value, Text *tp) {
    const char	*ks = StringValuePtr(key);
    int		klen = (int)RSTRING_LEN(key);
    const char	*vs = StringValuePtr(value);
    int		vlen = (int)RSTRING_LEN(value);

    http_header_ok(ks, klen, vs, vlen);
    if (0 != strcasecmp("Content-Length", ks)) {
	*tp = text_append(*tp, ks, klen);
	*tp = text_append(*tp, ": ", 2);
	*tp = text_append(*tp, vs, vlen);
	*tp = text_append(*tp, "\r\n", 2);
    }
    return ST_CONTINUE;
}

static VALUE
header_each_cb(VALUE kv, Text *tp) {
    header_cb(rb_ary_entry(kv, 0), rb_ary_entry(kv, 1), tp);

    return Qnil;
}

static VALUE
body_len_cb(VALUE v, int *sizep) {
    *sizep += (int)RSTRING_LEN(v);

    return Qnil;
}

static VALUE
body_append_cb(VALUE v, Text *tp) {
    *tp = text_append(*tp, StringValuePtr(v), (int)RSTRING_LEN(v));

    return Qnil;
}

static VALUE
handle_rack_inner(void *x) {
    Req			req = (Req)x;
    Text		t;
    volatile VALUE	env = request_env(req);
    volatile VALUE	res = rb_funcall(req->handler, call_id, 1, env);
    volatile VALUE	hv;
    volatile VALUE	bv;
    int			code;
    const char		*status_msg;
    int			bsize = 0;

    rb_check_type(res, T_ARRAY);
    if (3 != RARRAY_LEN(res)) {
	rb_raise(rb_eArgError, "a rack call() response must be an array of 3 members.");
    }
    hv = rb_ary_entry(res, 0);
    if (RUBY_T_FIXNUM == rb_type(hv)) {
	code = NUM2INT(hv);
    } else {
	code = NUM2INT(rb_funcall(hv, to_i_id, 0));
    }
    status_msg = http_code_message(code);
    if ('\0' == *status_msg) {
	rb_raise(rb_eArgError, "invalid rack call() response status code (%d).", code);
    }
    hv = rb_ary_entry(res, 1);
    if (!rb_respond_to(hv, each_id)) {
	rb_raise(rb_eArgError, "invalid rack call() response headers does not respond to each.");
    }
    bv = rb_ary_entry(res, 2);
    if (!rb_respond_to(bv, each_id)) {
	rb_raise(rb_eArgError, "invalid rack call() response body does not respond to each.");
    }
    if (NULL == (t = text_allocate(1024))) {
	rb_raise(rb_eArgError, "failed to allocate response.");
    }
    if (T_ARRAY == rb_type(bv)) {
	int	i;
	int	bcnt = (int)RARRAY_LEN(bv);

	for (i = 0; i < bcnt; i++) {
	    bsize += (int)RSTRING_LEN(rb_ary_entry(bv, i));
	}
    } else {
	if (HEAD == req->method) {
	    // Rack wraps the response in two layers, Rack::Lint and
	    // Rack::BodyProxy. It each is called on either with the HEAD
	    // method an exception is raised so the length can not be
	    // determined. This digs down to get the actual response so the
	    // length can be calculated. A very special case.
	    if (0 == strcmp("Rack::BodyProxy", rb_obj_classname(bv))) {
		volatile VALUE	body = rb_ivar_get(bv, rb_intern("@body"));

		if (Qnil != body) {
		    body = rb_ivar_get(body, rb_intern("@body"));
		}
		if (Qnil != body) {
		    body = rb_ivar_get(body, rb_intern("@body"));
		}
		if (rb_respond_to(body, rb_intern("each"))) {
		    rb_iterate(rb_each, body, body_len_cb, (VALUE)&bsize);
		}
	    } else {
		rb_iterate(rb_each, bv, body_len_cb, (VALUE)&bsize);
	    }
	} else {
	    rb_iterate(rb_each, bv, body_len_cb, (VALUE)&bsize);
	}
    }
    switch (code) {
    case 100:
    case 102:
    case 204:
    case 205:
    case 304:
	// Content-Type and Content-Length can not be present
	t->len = snprintf(t->text, 1024, "HTTP/1.1 %d %s\r\n", code, status_msg);
	break;
    case 101:

	//printf("*** protocol change  %c -> %c\n", req->upgrade, req->res->con_kind);
	
	switch (req->upgrade) {
	case UP_WS:
	    if (CON_WS != req->res->con_kind ||
		Qnil == (req->handler = rb_hash_lookup(env, push_env_key))) {
		strcpy(t->text, err500);
		t->len = sizeof(err500) - 1;
		break;
	    }
	    req->handler_type = PUSH_HOOK;
	    upgraded_extend(req->cid, req->handler);
	    t->len = snprintf(t->text, 1024, "HTTP/1.1 101 %s\r\n", status_msg);
	    t = ws_add_headers(req, t);
	    break;
	case UP_SSE:
	    // TBD
	    break;
	default:
	    // An error on the app's part as a protocol upgrade must include a
	    // supported handler.
	    strcpy(t->text, err500);
	    t->len = sizeof(err500) - 1;
	    break;
	}
	break;
    default:
	// Note that using simply sprintf causes an abort with travis OSX tests.
	t->len = snprintf(t->text, 1024, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n", code, status_msg, bsize);
	break;
    }
    if (HEAD == req->method) {
	bsize = 0;
    } else {
	if (T_HASH == rb_type(hv)) {
	    rb_hash_foreach(hv, header_cb, (VALUE)&t);
	} else {
	    rb_iterate(rb_each, hv, header_each_cb, (VALUE)&t);
	}
    }
    t = text_append(t, "\r\n", 2);
    if (0 < bsize) {
	if (T_ARRAY == rb_type(bv)) {
	    VALUE	v;
	    int		i;
	    int		bcnt = (int)RARRAY_LEN(bv);

	    for (i = 0; i < bcnt; i++) {
		v = rb_ary_entry(bv, i);
		t = text_append(t, StringValuePtr(v), (int)RSTRING_LEN(v));
	    }
	} else {
	    rb_iterate(rb_each, bv, body_append_cb, (VALUE)&t);
	}
    }
    res_set_message(req->res, t);
    queue_wakeup(&req->server->con_queue);

    return Qfalse;
}

static void*
handle_rack(void *x) {
    // Disable GC. The handle_rack function or rather the env seems to get
    // collected even though it is volatile so for now turn off GC
    // temporarily.
    //rb_gc_disable();
    rb_rescue2(handle_rack_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);
    //rb_gc_enable();
    //rb_gc();

    return NULL;
}

static VALUE
handle_wab_inner(void *x) {
    Req			req = (Req)x;
    volatile VALUE	rr = request_wrap(req);
    volatile VALUE	rres = response_new(req->server);
    
    rb_funcall(req->handler, on_request_id, 2, rr, rres);
    DATA_PTR(rr) = NULL;

    res_set_message(req->res, response_text(rres));
    queue_wakeup(&req->server->con_queue);

    return Qfalse;
}

static void*
handle_wab(void *x) {
    rb_rescue2(handle_wab_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);

    return NULL;
}

static VALUE
handle_push_inner(void *x) {
    Req	req = (Req)x;

    printf("****** handle_push %c - %c  %lx\n", req->handler_type, req->method, req->handler);

    switch (req->method) {
    case ON_MSG:
	// TBD handle binary as well
	rb_funcall(req->handler, rb_intern("on_message"), 1, rb_str_new(req->msg, req->mlen));
	break;
    case ON_CLOSE:
	rb_funcall(req->handler, rb_intern("on_close"), 0);
	break;
    case ON_SHUTDOWN:
	rb_funcall(req->handler, rb_intern("on_shutdown"), 0);
	break;
    case ON_EMPTY:
	rb_funcall(req->handler, rb_intern("on_drained"), 0);
	break;
    default:
	break;
    }
    // TBD

    return Qfalse;
}

static void*
handle_push(void *x) {
    rb_rescue2(handle_push_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);
    return NULL;
}

static void
handle_protected(Req req, bool gvi) {
    switch (req->handler_type) {
    case BASE_HOOK:
	if (gvi) {
	    rb_thread_call_with_gvl(handle_base, req);
	} else {
	    handle_base(req);
	}
	break;
    case RACK_HOOK:
	if (gvi) {
	    rb_thread_call_with_gvl(handle_rack, req);
	} else {
	    handle_rack(req);
	}
	break;
    case WAB_HOOK:
	if (gvi) {
	    rb_thread_call_with_gvl(handle_wab, req);
	} else {
	    handle_wab(req);
	}
	break;
    case PUSH_HOOK:
	if (gvi) {
	    rb_thread_call_with_gvl(handle_push, req);
	} else {
	    handle_push(req);
	}
	break;
    default: {
	char	buf[256];
	int	cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 500 Internal Error\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n");
	Text	message = text_create(buf, cnt);
	
	req->res->close = true;
	res_set_message(req->res, message);
	queue_wakeup(&req->server->con_queue);
	break;
    }
    }
}

static void*
process_loop(void *ptr) {
    Server	server = (Server)ptr;
    Req		req;
    
    atomic_fetch_add(&server->running, 1);
    while (server->active) {
	if (NULL != (req = (Req)queue_pop(&server->eval_queue, 0.1))) {
	    handle_protected(req, true);
	    free(req);
	    DEBUG_FREE(mem_req)
	}
    }
    atomic_fetch_sub(&server->running, 1);
    
    return NULL;
}

static VALUE
wrap_process_loop(void *ptr) {
    rb_thread_call_without_gvl(process_loop, ptr, RUBY_UBF_IO, NULL);
    return Qnil;
}

/* Document-method: start
 *
 * call-seq: start()
 *
 * Start the server.
 */
static VALUE
start(VALUE self) {
    Server	server = (Server)DATA_PTR(self);
    VALUE	*vp;
    int		i;
    double	giveup;
    
    server->active = true;
    server->ready = false;

    pthread_create(&server->listen_thread, NULL, listen_loop, server);
    pthread_create(&server->con_thread, NULL, con_loop, server);
    
    giveup = dtime() + 1.0;
    while (dtime() < giveup) {
	if (server->ready) {
	    break;
	}
	dsleep(0.001);
    }
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    if (server->info_cat.on) {
	VALUE	agoo = rb_const_get_at(rb_cObject, rb_intern("Agoo"));
	VALUE	v = rb_const_get_at(agoo, rb_intern("VERSION"));
					       
	log_cat(&server->info_cat, "Agoo %s listening on port %d.", StringValuePtr(v), server->port);
    }
    if (0 >= server->thread_cnt) {
	Req		req;

	while (server->active) {
	    if (NULL != (req = (Req)queue_pop(&server->eval_queue, 0.1))) {
		handle_protected(req, false);
		free(req);
		DEBUG_FREE(mem_req)
	    }
	}
    } else {
	server->eval_threads = ALLOC_N(VALUE, server->thread_cnt + 1);
	DEBUG_ALLOC(mem_eval_threads)

	for (i = server->thread_cnt, vp = server->eval_threads; 0 < i; i--, vp++) {
	    *vp = rb_thread_create(wrap_process_loop, server);
	}
	*vp = Qnil;
    }
    return Qnil;
}

/* Document-method: shutdown
 *
 * call-seq: shutdown()
 *
 * Shutdown the server. Logs and queues are flushed before shutting down.
 */
static VALUE
server_shutdown(VALUE self) {
    shutdown_server((Server)DATA_PTR(self));
    return Qnil;
}

/* Document-method: error?
 *
 * call-seq: error?()
 *
 * Returns true is errors are being logged.
 */
static VALUE
log_errorp(VALUE self) {
    return ((Server)DATA_PTR(self))->error_cat.on ? Qtrue : Qfalse;
}

/* Document-method: warn?
 *
 * call-seq: warn?()
 *
 * Returns true is warnings are being logged.
 */
static VALUE
log_warnp(VALUE self) {
    return ((Server)DATA_PTR(self))->warn_cat.on ? Qtrue : Qfalse;
}

/* Document-method: info?
 *
 * call-seq: info?()
 *
 * Returns true is info entries are being logged.
 */
static VALUE
log_infop(VALUE self) {
    return ((Server)DATA_PTR(self))->info_cat.on ? Qtrue : Qfalse;
}

/* Document-method: debug?
 *
 * call-seq: debug?()
 *
 * Returns true is debug entries are being logged.
 */
static VALUE
log_debugp(VALUE self) {
    return ((Server)DATA_PTR(self))->debug_cat.on ? Qtrue : Qfalse;
}

/* Document-method: eval?
 *
 * call-seq: eval?()
 *
 * Returns true is handler evaluation entries are being logged.
 */
static VALUE
log_evalp(VALUE self) {
    return ((Server)DATA_PTR(self))->eval_cat.on ? Qtrue : Qfalse;
}

/* Document-method: error
 *
 * call-seq: error(msg)
 *
 * Log an error message.
 */
static VALUE
log_error(VALUE self, VALUE msg) {
    log_cat(&((Server)DATA_PTR(self))->error_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: warn
 *
 * call-seq: warn(msg)
 *
 * Log a warn message.
 */
static VALUE
log_warn(VALUE self, VALUE msg) {
    log_cat(&((Server)DATA_PTR(self))->warn_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: info
 *
 * call-seq: info(msg)
 *
 * Log an info message.
 */
static VALUE
log_info(VALUE self, VALUE msg) {
    log_cat(&((Server)DATA_PTR(self))->info_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: debug
 *
 * call-seq: debug(msg)
 *
 * Log a debug message.
 */
static VALUE
log_debug(VALUE self, VALUE msg) {
    log_cat(&((Server)DATA_PTR(self))->debug_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: log_eval
 *
 * call-seq: log_eval(msg)
 *
 * Log an eval message.
 */
static VALUE
log_eval(VALUE self, VALUE msg) {
    log_cat(&((Server)DATA_PTR(self))->eval_cat, "%s", StringValuePtr(msg));
    return Qnil;
}

/* Document-method: log_state
 *
 * call-seq: log_state(label)
 *
 * Return the logging state of the category identified by the _label_.
 */
static VALUE
log_state(VALUE self, VALUE label) {
    Server	server = (Server)DATA_PTR(self);
    LogCat	cat = log_cat_find(&server->log, StringValuePtr(label));

    if (NULL == cat) {
	rb_raise(rb_eArgError, "%s is not a valid log category.", StringValuePtr(label));
    }
    return cat->on ? Qtrue : Qfalse;
}

/* Document-method: set_log_state
 *
 * call-seq: set_log_state(label, state)
 *
 * Set the logging state of the category identified by the _label_.
 */
static VALUE
set_log_state(VALUE self, VALUE label, VALUE on) {
    Server	server = (Server)DATA_PTR(self);
    LogCat	cat = log_cat_find(&server->log, StringValuePtr(label));

    if (NULL == cat) {
	rb_raise(rb_eArgError, "%s is not a valid log category.", StringValuePtr(label));
    }
    cat->on = (Qtrue == on);

    return Qnil;
}

/* Document-method: log_flush
 *
 * call-seq: log_flush()
 *
 * Flush the log queue and write all entries to disk or the console. The call
 * waits for the flush to complete.
 */
static VALUE
server_log_flush(VALUE self, VALUE to) {
    double	timeout = NUM2DBL(to);
    
    if (!log_flush(&((Server)DATA_PTR(self))->log, timeout)) {
	rb_raise(rb_eStandardError, "timed out waiting for log flush.");
    }
    return Qnil;
}

/* Document-method: handle
 *
 * call-seq: handle(method, pattern, handler)
 *
 * Registers a handler for the HTTP method and path pattern specified. The
 * path pattern follows glob like rules in that a single * matches a single
 * token bounded by the `/` character and a double ** matches all remaining.
 */
static VALUE
handle(VALUE self, VALUE method, VALUE pattern, VALUE handler) {
    Server	server = (Server)DATA_PTR(self);
    Hook	hook;
    Method	meth = ALL;
    const char	*pat;

    rb_check_type(pattern, T_STRING);
    pat = StringValuePtr(pattern);

    if (connect_sym == method) {
	meth = CONNECT;
    } else if (delete_sym == method) {
	meth = DELETE;
    } else if (get_sym == method) {
	meth = GET;
    } else if (head_sym == method) {
	meth = HEAD;
    } else if (options_sym == method) {
	meth = OPTIONS;
    } else if (post_sym == method) {
	meth = POST;
    } else if (put_sym == method) {
	meth = PUT;
    } else if (Qnil == method) {
	meth = ALL;
    } else {
	rb_raise(rb_eArgError, "invalid method");
    }
    if (NULL == (hook = hook_create(meth, pat, handler))) {
	rb_raise(rb_eStandardError, "out of memory.");
    } else {
	Hook	h;
	Hook	prev = NULL;

	for (h = server->hooks; NULL != h; h = h->next) {
	    prev = h;
	}
	if (NULL != prev) {
	    prev->next = hook;
	} else {
	    server->hooks = hook;
	}
	rb_gc_register_address(&hook->handler);
    }
    return Qnil;
}

/* Document-method: handle_not_found
 *
 * call-seq: not_found_handle(handler)
 *
 * Registers a handler to be called when no other hook is found and no static
 * file is found.
 */
static VALUE
handle_not_found(VALUE self, VALUE handler) {
    Server	server = (Server)DATA_PTR(self);

    if (NULL == (server->hook404 = hook_create(GET, "/", handler))) {
	rb_raise(rb_eStandardError, "out of memory.");
    }
    rb_gc_register_address(&server->hook404->handler);
    
    return Qnil;
}

/* Document-method: add_mime
 *
 * call-seq: add_mime(suffix, type)
 *
 * Adds a mime type by associating a type string with a suffix. This is used
 * for static files.
 */
static VALUE
add_mime(VALUE self, VALUE suffix, VALUE type) {
    mime_set(&((Server)DATA_PTR(self))->pages, StringValuePtr(suffix), StringValuePtr(type));

    return Qnil;
}

/* Document-class: Agoo::Server
 *
 * An HTTP server that support the rack API as well as some other optimized
 * APIs for handling HTTP requests.
 */
void
server_init(VALUE mod) {
    server_class = rb_define_class_under(mod, "Server", rb_cObject);

    rb_define_module_function(server_class, "new", server_new, -1);
    rb_define_method(server_class, "start", start, 0);
    rb_define_method(server_class, "shutdown", server_shutdown, 0);

    rb_define_method(server_class, "error?", log_errorp, 0);
    rb_define_method(server_class, "warn?", log_warnp, 0);
    rb_define_method(server_class, "info?", log_infop, 0);
    rb_define_method(server_class, "debug?", log_debugp, 0);
    rb_define_method(server_class, "log_eval?", log_evalp, 0);
    rb_define_method(server_class, "error", log_error, 1);
    rb_define_method(server_class, "warn", log_warn, 1);
    rb_define_method(server_class, "info", log_info, 1);
    rb_define_method(server_class, "debug", log_debug, 1);
    rb_define_method(server_class, "log_eval", log_eval, 1);

    rb_define_method(server_class, "log_state", log_state, 1);
    rb_define_method(server_class, "set_log_state", set_log_state, 2);
    rb_define_method(server_class, "log_flush", server_log_flush, 1);

    rb_define_method(server_class, "handle", handle, 3);
    rb_define_method(server_class, "handle_not_found", handle_not_found, 1);
    rb_define_method(server_class, "add_mime", add_mime, 2);

    call_id = rb_intern("call");
    each_id = rb_intern("each");
    on_request_id = rb_intern("on_request");
    to_i_id = rb_intern("to_i");
    
    connect_sym = ID2SYM(rb_intern("CONNECT"));		rb_gc_register_address(&connect_sym);
    delete_sym = ID2SYM(rb_intern("DELETE"));		rb_gc_register_address(&delete_sym);
    get_sym = ID2SYM(rb_intern("GET"));			rb_gc_register_address(&get_sym);
    head_sym = ID2SYM(rb_intern("HEAD"));		rb_gc_register_address(&head_sym);
    options_sym = ID2SYM(rb_intern("OPTIONS"));		rb_gc_register_address(&options_sym);
    post_sym = ID2SYM(rb_intern("POST"));		rb_gc_register_address(&post_sym);
    put_sym = ID2SYM(rb_intern("PUT"));			rb_gc_register_address(&put_sym);

    push_env_key = rb_str_new_cstr("rack.upgrade");	rb_gc_register_address(&push_env_key);

    http_init();
}
