// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>

#include <ruby.h>
#include <ruby/thread.h>
#include <ruby/encoding.h>

#include "bind.h"
#include "con.h"
#include "debug.h"
#include "dtime.h"
#include "err.h"
#include "http.h"
#include "log.h"
#include "page.h"
#include "pub.h"
#include "response.h"
#include "request.h"
#include "rhook.h"
#include "rserver.h"
#include "rresponse.h"
#include "rupgraded.h"
#include "server.h"
#include "sse.h"
#include "sub.h"
#include "upgraded.h"
#include "websocket.h"

extern void	agoo_shutdown();

static VALUE	server_mod = Qundef;

static VALUE	connect_sym;
static VALUE	delete_sym;
static VALUE	get_sym;
static VALUE	head_sym;
static VALUE	options_sym;
static VALUE	post_sym;
static VALUE	push_env_key;
static VALUE	put_sym;

static VALUE	rserver;

static ID	call_id;
static ID	each_id;
static ID	on_close_id;
static ID	on_drained_id;
static ID	on_error_id;
static ID	on_message_id;
static ID	on_request_id;
static ID	to_i_id;

static const char	err500[] = "HTTP/1.1 500 Internal Server Error\r\n";

struct _RServer	the_rserver = {};

static void
server_mark(void *ptr) {
    Upgraded	up;

    rb_gc_mark(rserver);
    pthread_mutex_lock(&the_server.up_lock);
    for (up = the_server.up_list; NULL != up; up = up->next) {
	if (Qnil != (VALUE)up->ctx) {
	    rb_gc_mark((VALUE)up->ctx);
	}
	if (Qnil != (VALUE)up->env) {
	    rb_gc_mark((VALUE)up->env);
	}
	if (Qnil != (VALUE)up->wrap) {
	    rb_gc_mark((VALUE)up->wrap);
	}
    }
    pthread_mutex_unlock(&the_server.up_lock);
}

static void
url_bind(VALUE rurl) {
    struct _Err	err = ERR_INIT;
    Bind	b = bind_url(&err, StringValuePtr(rurl));

    if (ERR_OK != err.code) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    server_bind(b);
}

static int
configure(Err err, int port, const char *root, VALUE options) {
    pages_set_root(root);
    the_server.thread_cnt = 0;
    the_rserver.worker_cnt = 1;
    the_server.running = 0;
    the_server.listen_thread = 0;
    the_server.con_thread = 0;
    the_server.root_first = false;
    the_server.binds = NULL;

    if (Qnil != options) {
	VALUE	v;

	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("thread_count"))))) {
	    int	tc = FIX2INT(v);

	    if (1 <= tc || tc < 1000) {
		the_server.thread_cnt = tc;
	    } else {
		rb_raise(rb_eArgError, "thread_count must be between 1 and 1000.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("worker_count"))))) {
	    int	wc = FIX2INT(v);

	    if (1 <= wc || wc < MAX_WORKERS) {
		the_rserver.worker_cnt = wc;
	    } else {
		rb_raise(rb_eArgError, "thread_count must be between 1 and %d.", MAX_WORKERS);
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("max_push_pending"))))) {
	    int	mpp = FIX2INT(v);

	    if (0 <= mpp || mpp < 1000) {
		the_server.max_push_pending = mpp;
	    } else {
		rb_raise(rb_eArgError, "max_push_pending must be between 0 and 1000.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("pedantic"))))) {
	    the_server.pedantic = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("root_first"))))) {
	    the_server.root_first = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("Port"))))) {
	    if (rb_cInteger == rb_obj_class(v)) {
		port = NUM2INT(v);
	    } else {
		switch (rb_type(v)) {
		case T_STRING:
		    port = atoi(StringValuePtr(v));
		    break;
		case T_FIXNUM:
		    port = NUM2INT(v);
		    break;
		default:
		    break;
		}
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("bind"))))) {
	    int	len;
	    int	i;
	    
	    switch (rb_type(v)) {
	    case T_STRING:
		url_bind(v);
		break;
	    case T_ARRAY:
		len = RARRAY_LEN(v);
		for (i = 0; i < len; i++) {
		    url_bind(rb_ary_entry(v, i));
		}
		break;
	    default:	
		rb_raise(rb_eArgError, "bind option must be a String or Array of Strings.");
		break;
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("quiet"))))) {
	    if (Qtrue == v) {
		info_cat.on = false;
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("debug"))))) {
	    if (Qtrue == v) {
		error_cat.on = true;
		warn_cat.on = true;
		info_cat.on = true;
		debug_cat.on = true;
		con_cat.on = true;
		req_cat.on = true;
		resp_cat.on = true;
		eval_cat.on = true;
		push_cat.on = true;
	    }
	}
    }
    if (0 < port) {
	Bind	b = bind_port(err, port);

	if (ERR_OK != err->code) {
	    rb_raise(rb_eArgError, "%s", err->msg);
	}
	server_bind(b);
    }
    return ERR_OK;
}

/* Document-method: init
 *
 * call-seq: init(port, root, options)
 *
 * Configures the server that will listen on the designated _port_ and using
 * the _root_ as the root of the static resources. Logging is feature based
 * and not level based and the options reflect that approach. If bind option
 * is to be used instead of the port then set the port to zero.
 *
 * - *options* [_Hash_] server options
 *
 *   - *:pedantic* [_true_|_false_] if true response header and status codes are checked and an exception raised if they violate the rack spec at https://github.com/rack/rack/blob/master/SPEC, https://tools.ietf.org/html/rfc3875#section-4.1.18, or https://tools.ietf.org/html/rfc7230.
 *
 *   - *:thread_count* [_Integer_] number of ruby worker threads. Defaults to one. If zero then the _start_ function will not return but instead will proess using the thread that called _start_. Usually the default is best unless the workers are making IO calls.
 *
 *   - *:worker_count* [_Integer_] number of workers to fork. Defaults to one which is not to fork.
 *
 *   - *:bind* [_String_|_Array_] a binding or array of binds. Examples are: "http://127.0.0.1:6464", "unix:///tmp/agoo.socket", "http://[::1]:6464, or to not restrict the address "http://:6464".
 */
static VALUE
rserver_init(int argc, VALUE *argv, VALUE self) {
    struct _Err	err = ERR_INIT;
    int		port;
    const char	*root;
    VALUE	options = Qnil;

    if (argc < 2 || 3 < argc) {
	rb_raise(rb_eArgError, "Wrong number of arguments to Agoo::Server.configure.");
    }
    port = FIX2INT(argv[0]);
    rb_check_type(argv[1], T_STRING);
    root = StringValuePtr(argv[1]);
    if (3 <= argc) {
	options = argv[2];
    }
    server_setup();
    the_server.ctx_nil_value = (void*)Qnil;
    the_server.env_nil_value = (void*)Qnil;

    if (ERR_OK != configure(&err, port, root, options)) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    the_server.inited = true;

    return Qnil;
}

static const char	bad500[] = "HTTP/1.1 500 Internal Error\r\nConnection: Close\r\nContent-Length: ";

static VALUE
rescue_error(VALUE x) {
    Req			req = (Req)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);

    if (NULL == req->up) {
	char	buf[1024];
	int	len = (int)(strlen(classname) + 2 + strlen(ms));
	int	cnt;
	Text	message;

	if ((int)(sizeof(buf) - sizeof(bad500) + 7) <= len) {
	    len = sizeof(buf) - sizeof(bad500) + 7;
	}
	cnt = snprintf(buf, sizeof(buf), "%s%d\r\n\r\n%s: %s", bad500, len, classname, ms);
	message = text_create(buf, cnt);

	req->res->close = true;
	res_set_message(req->res, message);
	queue_wakeup(&the_server.con_queue);
    } else {
/*
	volatile VALUE	bt = rb_funcall(info, rb_intern("backtrace"), 0);
	int		blen = RARRAY_LEN(bt);
	int		i;
	VALUE		rline;
	
	for (i = 0; i < blen; i++) {
	    rline = rb_ary_entry(bt, i);
	}
*/
	log_cat(&error_cat, "%s: %s", classname, ms);
    }
    return Qfalse;
}

static VALUE
handle_base_inner(void *x) {
    Req			req = (Req)x;
    volatile VALUE	rr = request_wrap(req);
    volatile VALUE	rres = response_new();

    if (NULL != req->hook) {
	rb_funcall((VALUE)req->hook->handler, on_request_id, 2, rr, rres);
    }
    DATA_PTR(rr) = NULL;
    res_set_message(req->res, response_text(rres));
    queue_wakeup(&the_server.con_queue);

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
    struct _Err	err = ERR_INIT;
    
    if (the_server.pedantic) {
	if (ERR_OK != http_header_ok(&err, ks, klen, vs, vlen)) {
	    rb_raise(rb_eArgError, "%s", err.msg);
	}
    }
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
    volatile VALUE	env = request_env(req, request_wrap(req));
    volatile VALUE	res = Qnil;
    volatile VALUE	hv;
    volatile VALUE	bv;
    int			code;
    const char		*status_msg;
    int			bsize = 0;

    if (NULL == req->hook) {
	return Qfalse;
    }
    res = rb_funcall((VALUE)req->hook->handler, call_id, 1, env);
    if (req->res->con->hijacked) {
	queue_wakeup(&the_server.con_queue);
	return Qfalse;
    }
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
    case 101:
    case 102:
    case 204:
    case 205:
    case 304:
	// Content-Type and Content-Length can not be present
	t->len = snprintf(t->text, 1024, "HTTP/1.1 %d %s\r\n", code, status_msg);
	break;
    default:
	// Note that using simply sprintf causes an abort with travis OSX tests.
	t->len = snprintf(t->text, 1024, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n", code, status_msg, bsize);
	break;
    }
    if (code < 300) {
	VALUE	handler = Qnil;

	switch (req->upgrade) {
	case UP_WS:
	    if (CON_WS != req->res->con_kind ||
		Qnil == (handler = rb_hash_lookup(env, push_env_key))) {
		strcpy(t->text, err500);
		t->len = sizeof(err500) - 1;
		break;
	    }
	    req->hook = hook_create(NONE, NULL, (void*)handler, PUSH_HOOK, &the_server.eval_queue);
	    rupgraded_create(req->res->con, handler, request_env(req, Qnil));

	    t->len = snprintf(t->text, 1024, "HTTP/1.1 101 %s\r\n", status_msg);
	    t = ws_add_headers(req, t);
	    break;
	case UP_SSE:
	    if (CON_SSE != req->res->con_kind ||
		Qnil == (handler = rb_hash_lookup(env, push_env_key))) {
		strcpy(t->text, err500);
		t->len = sizeof(err500) - 1;
		break;
	    }
	    req->hook = hook_create(NONE, NULL, (void*)handler, PUSH_HOOK, &the_server.eval_queue);
	    rupgraded_create(req->res->con, handler, request_env(req, Qnil));
	    t = sse_upgrade(req, t);
	    res_set_message(req->res, t);
	    queue_wakeup(&the_server.con_queue);
	    return Qfalse;
	default:
	    break;
	}
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
    queue_wakeup(&the_server.con_queue);

    return Qfalse;
}

static void*
handle_rack(void *x) {
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
    volatile VALUE	rres = response_new();

    if (NULL != req->hook) {
	rb_funcall((VALUE)req->hook->handler, on_request_id, 2, rr, rres);
    }
    DATA_PTR(rr) = NULL;
    res_set_message(req->res, response_text(rres));
    queue_wakeup(&the_server.con_queue);

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

    switch (req->method) {
    case ON_MSG:
	if (req->up->on_msg && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_message_id, 2, (VALUE)req->up->wrap, rb_str_new(req->msg, req->mlen));
	}
	break;
    case ON_BIN:
	if (req->up->on_msg && NULL != req->hook) {
	    volatile VALUE	rstr = rb_str_new(req->msg, req->mlen);

	    rb_enc_associate(rstr, rb_ascii8bit_encoding());
	    rb_funcall((VALUE)req->hook->handler, on_message_id, 2, (VALUE)req->up->wrap, rstr);
	}
	break;
    case ON_CLOSE:
	upgraded_ref(req->up);
	queue_push(&the_server.pub_queue, pub_close(req->up));
	if (req->up->on_close && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_close_id, 1, (VALUE)req->up->wrap);
	}
	break;
    case ON_SHUTDOWN:
	if (NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, rb_intern("on_shutdown"), 1, (VALUE)req->up->wrap);
	}
	break;
    case ON_ERROR:
	if (req->up->on_error && NULL != req->hook) {
	    volatile VALUE	rstr = rb_str_new(req->msg, req->mlen);

	    rb_enc_associate(rstr, rb_ascii8bit_encoding());
	    rb_funcall((VALUE)req->hook->handler, on_error_id, 2, (VALUE)req->up->wrap, rstr);
	}
	break;
    case ON_EMPTY:
	if (req->up->on_empty && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_drained_id, 1, (VALUE)req->up->wrap);
	}
	break;
    default:
	break;
    }
    upgraded_release(req->up);
    
    return Qfalse;
}

static void*
handle_push(void *x) {
    rb_rescue2(handle_push_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);
    return NULL;
}

static void
handle_protected(Req req, bool gvi) {
    if (NULL == req->hook) {
	return;
    }
    switch (req->hook->type) {
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
	queue_wakeup(&the_server.con_queue);
	break;
    }
    }
}

static void*
process_loop(void *ptr) {
    Req	req;
    
    atomic_fetch_add(&the_server.running, 1);
    while (the_server.active) {
	if (NULL != (req = (Req)queue_pop(&the_server.eval_queue, 0.1))) {
	    handle_protected(req, true);
	    req_destroy(req);
	}
    }
    atomic_fetch_sub(&the_server.running, 1);

    return NULL;
}

static VALUE
wrap_process_loop(void *ptr) {
    rb_thread_call_without_gvl(process_loop, NULL, RUBY_UBF_IO, NULL);
    return Qnil;
}

/* Document-method: start
 *
 * call-seq: start()
 *
 * Start the server.
 */
static VALUE
rserver_start(VALUE self) {
    VALUE	*vp;
    int		i;
    int		pid;
    double	giveup;
    struct _Err	err = ERR_INIT;
    VALUE	agoo = rb_const_get_at(rb_cObject, rb_intern("Agoo"));
    VALUE	v = rb_const_get_at(agoo, rb_intern("VERSION"));
    
    *the_rserver.worker_pids = getpid();

    if (ERR_OK != setup_listen(&err)) {
	rb_raise(rb_eIOError, "%s", err.msg);
    }
    for (i = 1; i < the_rserver.worker_cnt; i++) {
	VALUE	rpid = rb_funcall(rb_cObject, rb_intern("fork"), 0);

	if (Qnil == rpid) {
	    pid = 0;
	} else {
	    pid = NUM2INT(rpid);
	}
	if (0 > pid) { // error, use single process
	    log_cat(&error_cat, "Failed to fork. %s.", strerror(errno));
	    break;
	} else if (0 == pid) {
	    log_start(true);
	    break;
	} else {
	    the_rserver.worker_pids[i] = pid;
	}
    }
    if (ERR_OK != server_start(&err, "Agoo", StringValuePtr(v))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (0 >= the_server.thread_cnt) {
	Req		req;

	while (the_server.active) {
	    if (NULL != (req = (Req)queue_pop(&the_server.eval_queue, 0.1))) {
		handle_protected(req, false);
		req_destroy(req);
	    } else {
		rb_thread_schedule();
	    }

	}
    } else {
	the_rserver.eval_threads = (VALUE*)malloc(sizeof(VALUE) * (the_server.thread_cnt + 1));
	DEBUG_ALLOC(mem_eval_threads, the_server.eval_threads);

	for (i = the_server.thread_cnt, vp = the_rserver.eval_threads; 0 < i; i--, vp++) {
	    *vp = rb_thread_create(wrap_process_loop, NULL);
	}
	*vp = Qnil;

	giveup = dtime() + 1.0;
	while (dtime() < giveup) {
	    // The processing threads will not start until this thread
	    // releases ownership so do that and then see if the threads has
	    // been started yet.
	    rb_thread_schedule();
	    if (2 + the_server.thread_cnt <= atomic_load(&the_server.running)) {
		break;
	    }
	}
    }
    return Qnil;
}

static void
stop_runners() {
    // The preferred method of waiting for the ruby threads would be either a
    // join or even a kill but since we may not have the gvl here that would
    // cause a segfault. Instead we set a timeout and wait for the running
    // counter to drop to zero.
    if (NULL != the_rserver.eval_threads) {
	double	timeout = dtime() + 2.0;

	while (dtime() < timeout) {
	    if (0 >= atomic_load(&the_server.running)) {
		break;
	    }
	    dsleep(0.02);
	}
	DEBUG_FREE(mem_eval_threads, the_rserver.eval_threads);
	free(the_rserver.eval_threads);
	the_rserver.eval_threads = NULL;
    }
}

/* Document-method: shutdown
 *
 * call-seq: shutdown()
 *
 * Shutdown the server. Logs and queues are flushed before shutting down.
 */
VALUE
rserver_shutdown(VALUE self) {
    if (the_server.inited) {
	server_shutdown("Agoo", stop_runners);

	if (1 < the_rserver.worker_cnt && getpid() == *the_rserver.worker_pids) {
	    int	i;
	    int	status;
	    int	exit_cnt = 1;
	    int	j;
	    
	    for (i = 1; i < the_rserver.worker_cnt; i++) {
		kill(the_rserver.worker_pids[i], SIGKILL);
	    }
	    for (j = 0; j < 20; j++) {
		for (i = 1; i < the_rserver.worker_cnt; i++) {
		    if (0 == the_rserver.worker_pids[i]) {
			continue;
		    }
		    if (0 < waitpid(the_rserver.worker_pids[i], &status, WNOHANG)) {
			if (WIFEXITED(status)) {
			    //printf("exited, status=%d for %d\n", the_server.worker_pids[i], WEXITSTATUS(status));
			    the_rserver.worker_pids[i] = 0;
			    exit_cnt++;
			} else if (WIFSIGNALED(status)) {
			    //printf("*** killed by signal %d for %d\n", the_server.worker_pids[i], WTERMSIG(status));
			    the_rserver.worker_pids[i] = 0;
			    exit_cnt++;
			}
		    }
		}
		if (the_rserver.worker_cnt <= exit_cnt) {
		    break;
		}
		dsleep(0.2);
	    }
	    if (exit_cnt < the_rserver.worker_cnt) {
		printf("*-*-* Some workers did not exit.\n");
	    }
	}
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
    if (NULL == (hook = rhook_create(meth, pat, handler, &the_server.eval_queue))) {
	rb_raise(rb_eStandardError, "out of memory.");
    } else {
	Hook	h;
	Hook	prev = NULL;

	for (h = the_server.hooks; NULL != h; h = h->next) {
	    prev = h;
	}
	if (NULL != prev) {
	    prev->next = hook;
	} else {
	    the_server.hooks = hook;
	}
	rb_gc_register_address((VALUE*)&hook->handler);
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
    if (NULL == (the_server.hook404 = rhook_create(GET, "/", handler, &the_server.eval_queue))) {
	rb_raise(rb_eStandardError, "out of memory.");
    }
    rb_gc_register_address((VALUE*)&the_server.hook404->handler);
    
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
    struct _Err	err = ERR_INIT;
    
    if (ERR_OK != mime_set(&err, StringValuePtr(suffix), StringValuePtr(type))) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    return Qnil;
}

/* Document-method: path_group
 *
 * call-seq: path_group(path, dirs)
 *
 * Sets up a path group where the path defines a group of directories to
 * search for a file. For example a path of '/assets' could be mapped to a set
 * of [ 'home/user/images', '/home/user/app/assets/images' ].
 */
static VALUE
path_group(VALUE self, VALUE path, VALUE dirs) {
    Group	g;

    rb_check_type(path, T_STRING);
    rb_check_type(dirs, T_ARRAY);

    if (NULL != (g = group_create(StringValuePtr(path)))) {
	int	i;
	int	dcnt = (int)RARRAY_LEN(dirs);
	VALUE	entry;
	
	for (i = dcnt - 1; 0 <= i; i--) {
	    entry = rb_ary_entry(dirs, i);
	    if (T_STRING != rb_type(entry)) {
		entry = rb_funcall(entry, rb_intern("to_s"), 0);
	    }
	    group_add(g, StringValuePtr(entry));
	}
    }
    return Qnil;
}

/* Document-class: Agoo::Server
 *
 * An HTTP server that support the rack API as well as some other optimized
 * APIs for handling HTTP requests.
 */
void
server_init(VALUE mod) {
    server_mod = rb_define_module_under(mod, "Server");

    rb_define_module_function(server_mod, "init", rserver_init, -1);
    rb_define_module_function(server_mod, "start", rserver_start, 0);
    rb_define_module_function(server_mod, "shutdown", rserver_shutdown, 0);

    rb_define_module_function(server_mod, "handle", handle, 3);
    rb_define_module_function(server_mod, "handle_not_found", handle_not_found, 1);
    rb_define_module_function(server_mod, "add_mime", add_mime, 2);
    rb_define_module_function(server_mod, "path_group", path_group, 2);

    call_id = rb_intern("call");
    each_id = rb_intern("each");
    on_close_id = rb_intern("on_close");
    on_drained_id = rb_intern("on_drained");
    on_error_id = rb_intern("on_error");
    on_message_id = rb_intern("on_message");
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

    rserver = Data_Wrap_Struct(rb_cObject, server_mark, NULL, strdup("dummy"));
    rb_gc_register_address(&rserver);

    http_init();
}
