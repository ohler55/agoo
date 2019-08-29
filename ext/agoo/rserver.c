// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include <ruby.h>
#include <ruby/thread.h>
#include <ruby/encoding.h>

#include "atomic.h"
#include "bind.h"
#include "con.h"
#include "debug.h"
#include "domain.h"
#include "dtime.h"
#include "err.h"
#include "graphql.h"
#include "http.h"
#include "log.h"
#include "page.h"
#include "pub.h"
#include "request.h"
#include "res.h"
#include "response.h"
#include "rhook.h"
#include "rresponse.h"
#include "rserver.h"
#include "rupgraded.h"
#include "server.h"
#include "sse.h"
#include "upgraded.h"
#include "websocket.h"

extern void		agoo_shutdown();
extern sig_atomic_t	agoo_stop;

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

struct _rServer	the_rserver = {};

static void
server_mark(void *ptr) {
    agooUpgraded	up;

    rb_gc_mark(rserver);
    pthread_mutex_lock(&agoo_server.up_lock);
    for (up = agoo_server.up_list; NULL != up; up = up->next) {
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
    pthread_mutex_unlock(&agoo_server.up_lock);
}

static void
url_bind(VALUE rurl) {
    struct _agooErr	err = AGOO_ERR_INIT;
    agooBind		b = agoo_bind_url(&err, StringValuePtr(rurl));

    if (AGOO_ERR_OK != err.code) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    agoo_server_bind(b);
}

static int
configure(agooErr err, int port, const char *root, VALUE options) {
    if (AGOO_ERR_OK != agoo_pages_set_root(err, root)) {
	return err->code;
    }
    agoo_server.thread_cnt = 0;
    the_rserver.worker_cnt = 1;
    atomic_init(&agoo_server.running, 0);
    agoo_server.listen_thread = 0;
    agoo_server.con_loops = NULL;
    agoo_server.root_first = false;
    agoo_server.binds = NULL;

    if (Qnil != options) {
	VALUE	v;

	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("thread_count"))))) {
	    int	tc = FIX2INT(v);

	    if (1 <= tc || tc < 1000) {
		agoo_server.thread_cnt = tc;
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
		agoo_server.max_push_pending = mpp;
	    } else {
		rb_raise(rb_eArgError, "max_push_pending must be between 0 and 1000.");
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("pedantic"))))) {
	    agoo_server.pedantic = (Qtrue == v);
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("root_first"))))) {
	    agoo_server.root_first = (Qtrue == v);
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
		len = (int)RARRAY_LEN(v);
		for (i = 0; i < len; i++) {
		    url_bind(rb_ary_entry(v, i));
		}
		break;
	    default:
		rb_raise(rb_eArgError, "bind option must be a String or Array of Strings.");
		break;
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("graphql"))))) {
	    const char	*path;
	    agooHook	dump_hook;
	    agooHook	get_hook;
	    agooHook	post_hook;
	    char	schema_path[256];
	    long	plen;

	    rb_check_type(v, T_STRING);
	    if (AGOO_ERR_OK != gql_init(err)) {
		return err->code;
	    }
	    path = StringValuePtr(v);
	    plen = (long)RSTRING_LEN(v);
	    if ((int)sizeof(schema_path) - 8 < plen) {
		rb_raise(rb_eArgError, "A graphql schema path is limited to %d characters.", (int)(sizeof(schema_path) - 8));
	    }
	    memcpy(schema_path, path, plen);
	    memcpy(schema_path + plen, "/schema", 8);

	    dump_hook = agoo_hook_func_create(AGOO_GET, schema_path, gql_dump_hook, &agoo_server.eval_queue);
	    get_hook = agoo_hook_func_create(AGOO_GET, path, gql_eval_get_hook, &agoo_server.eval_queue);
	    post_hook = agoo_hook_func_create(AGOO_POST, path, gql_eval_post_hook, &agoo_server.eval_queue);
	    dump_hook->next = get_hook;
	    get_hook->next = post_hook;
	    post_hook->next = agoo_server.hooks;
	    agoo_server.hooks = dump_hook;
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("quiet"))))) {
	    if (Qtrue == v) {
		agoo_info_cat.on = false;
	    }
	}
	if (Qnil != (v = rb_hash_lookup(options, ID2SYM(rb_intern("debug"))))) {
	    if (Qtrue == v) {
		agoo_error_cat.on = true;
		agoo_warn_cat.on = true;
		agoo_info_cat.on = true;
		agoo_debug_cat.on = true;
		agoo_con_cat.on = true;
		agoo_req_cat.on = true;
		agoo_resp_cat.on = true;
		agoo_eval_cat.on = true;
		agoo_push_cat.on = true;
	    }
	}
    }
    if (0 < port) {
	agooBind	b = agoo_bind_port(err, port);

	if (AGOO_ERR_OK != err->code) {
	    rb_raise(rb_eArgError, "%s", err->msg);
	}
	agoo_server_bind(b);
    }
    return AGOO_ERR_OK;
}

/* Document-method: init
 *
 * call-seq: init(port, root, options)
 *
 * Configures the server that will listen on the designated _port_ and using
 * the _root_ as the root of the static resources. This must be called before
 * using other server methods. Logging is feature based and not level based
 * and the options reflect that approach. If bind option is to be used instead
 * of the port then set the port to zero.
 *
 * - *options* [_Hash_] server options
 *
 *   - *:pedantic* [_true_|_false_] if true response header and status codes are checked and an exception raised if they violate the rack spec at https://github.com/rack/rack/blob/master/SPEC, https://tools.ietf.org/html/rfc3875#section-4.1.18, or https://tools.ietf.org/html/rfc7230.
 *
 *   - *:thread_count* [_Integer_] number of ruby worker threads. Defaults to one. If zero then the _start_ function will not return but instead will proess using the thread that called _start_. Usually the default is best unless the workers are making IO calls.
 *
 *   - *:worker_count* [_Integer_] number of workers to fork. Defaults to one which is not to fork.
 *
 *   - *:bind* [_String_|_Array_] a binding or array of binds. Examples are: "http ://127.0.0.1:6464", "unix:///tmp/agoo.socket", "http ://[::1]:6464, or to not restrict the address "http ://:6464".
 *
 *   - *:graphql* [_String_] path to GraphQL endpoint if support for GraphQL is desired.
 */
static VALUE
rserver_init(int argc, VALUE *argv, VALUE self) {
    struct _agooErr	err = AGOO_ERR_INIT;
    int			port;
    const char		*root;
    VALUE		options = Qnil;

    if (argc < 2 || 3 < argc) {
	rb_raise(rb_eArgError, "Wrong number of arguments to Agoo::Server.configure.");
    }
    port = FIX2INT(argv[0]);
    rb_check_type(argv[1], T_STRING);
    root = StringValuePtr(argv[1]);
    if (3 <= argc) {
	options = argv[2];
    }
    if (AGOO_ERR_OK != agoo_server_setup(&err)) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    agoo_server.ctx_nil_value = (void*)Qnil;
    agoo_server.env_nil_value = (void*)Qnil;

    if (AGOO_ERR_OK != configure(&err, port, root, options)) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    agoo_server.inited = true;

    return Qnil;
}

static const char	bad500[] = "HTTP/1.1 500 Internal Error\r\nConnection: Close\r\nContent-Length: ";

static VALUE
rescue_error(VALUE x) {
    agooReq		req = (agooReq)x;
    volatile VALUE	info = rb_errinfo();
    volatile VALUE	msg = rb_funcall(info, rb_intern("message"), 0);
    const char		*classname = rb_obj_classname(info);
    const char		*ms = rb_string_value_ptr(&msg);

    if (NULL == req->up) {
	char	buf[1024];
	int	len = (int)(strlen(classname) + 2 + strlen(ms));
	int	cnt;
	agooText	message;

	if ((int)(sizeof(buf) - sizeof(bad500) + 7) <= len) {
	    len = sizeof(buf) - sizeof(bad500) + 7;
	}
	cnt = snprintf(buf, sizeof(buf), "%s%d\r\n\r\n%s: %s", bad500, len, classname, ms);
	message = agoo_text_create(buf, cnt);

	req->res->close = true;
	agoo_res_message_push(req->res, message);
	agoo_queue_wakeup(&agoo_server.con_queue);
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
	agoo_log_cat(&agoo_error_cat, "%s: %s", classname, ms);
    }
    return Qfalse;
}

static VALUE
handle_base_inner(void *x) {
    agooReq		req = (agooReq)x;
    volatile VALUE	rr = request_wrap(req);
    volatile VALUE	rres = response_new();

    if (NULL != req->hook) {
	rb_funcall((VALUE)req->hook->handler, on_request_id, 2, rr, rres);
    }
    DATA_PTR(rr) = NULL;
    agoo_res_message_push(req->res, response_text(rres));
    agoo_queue_wakeup(&agoo_server.con_queue);

    return Qfalse;
}

static void*
handle_base(void *x) {
    rb_rescue2(handle_base_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);

    return NULL;
}

static int
header_cb(VALUE key, VALUE value, agooText *tp) {
    const char		*ks = StringValuePtr(key);
    int			klen = (int)RSTRING_LEN(key);
    const char		*vs = StringValuePtr(value);
    int			vlen = (int)RSTRING_LEN(value);
    struct _agooErr	err = AGOO_ERR_INIT;

    if (agoo_server.pedantic) {
	if (AGOO_ERR_OK != agoo_http_header_ok(&err, ks, klen, vs, vlen)) {
	    rb_raise(rb_eArgError, "%s", err.msg);
	}
    }
    if (0 != strcasecmp("Content-Length", ks)) {
	char	*end = index(vs, '\n');

	do {
	    end = index(vs, '\n');
	    *tp = agoo_text_append(*tp, ks, klen);
	    *tp = agoo_text_append(*tp, ": ", 2);
	    if (NULL == end) {
		if (0 < vlen) {
		    *tp = agoo_text_append(*tp, vs, vlen);
		}
	    } else {
		if (vs < end) {
		    *tp = agoo_text_append(*tp, vs, end - vs);
		}
		vlen -= end - vs + 1;
		vs = end + 1;
	    }
	    *tp = agoo_text_append(*tp, "\r\n", 2);
	} while (NULL != end && 0 < vlen);
    }
    return ST_CONTINUE;
}

static VALUE
header_each_cb(VALUE kv, agooText *tp) {
    header_cb(rb_ary_entry(kv, 0), rb_ary_entry(kv, 1), tp);

    return Qnil;
}

static VALUE
body_len_cb(VALUE v, int *sizep) {
    *sizep += (int)RSTRING_LEN(v);

    return Qnil;
}

static VALUE
body_append_cb(VALUE v, agooText *tp) {
    *tp = agoo_text_append(*tp, StringValuePtr(v), (int)RSTRING_LEN(v));

    return Qnil;
}

static VALUE
handle_rack_inner(void *x) {
    agooReq		req = (agooReq)x;
    agooText		t;
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
	agoo_queue_wakeup(&agoo_server.con_queue);
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
    status_msg = agoo_http_code_message(code);
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
    if (NULL == (t = agoo_text_allocate(1024))) {
	rb_raise(rb_eNoMemError, "Failed to allocate memory for a response.");
    }
    if (T_ARRAY == rb_type(bv)) {
	int	i;
	int	bcnt = (int)RARRAY_LEN(bv);

	for (i = 0; i < bcnt; i++) {
	    bsize += (int)RSTRING_LEN(rb_ary_entry(bv, i));
	}
    } else {
	if (AGOO_HEAD == req->method) {
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
	case AGOO_UP_WS:
	    if (AGOO_CON_WS != req->res->con_kind ||
		Qnil == (handler = rb_hash_lookup(env, push_env_key))) {
		strcpy(t->text, err500);
		t->len = sizeof(err500) - 1;
		break;
	    }
	    req->hook = agoo_hook_create(AGOO_NONE, NULL, (void*)handler, PUSH_HOOK, &agoo_server.eval_queue);
	    rupgraded_create(req->res->con, handler, request_env(req, Qnil));
	    t->len = snprintf(t->text, 1024, "HTTP/1.1 101 %s\r\n", status_msg);
	    t = agoo_ws_add_headers(req, t);
	    break;
	case AGOO_UP_SSE:
	    if (AGOO_CON_SSE != req->res->con_kind ||
		Qnil == (handler = rb_hash_lookup(env, push_env_key))) {
		strcpy(t->text, err500);
		t->len = sizeof(err500) - 1;
		break;
	    }
	    req->hook = agoo_hook_create(AGOO_NONE, NULL, (void*)handler, PUSH_HOOK, &agoo_server.eval_queue);
	    rupgraded_create(req->res->con, handler, request_env(req, Qnil));
	    t = agoo_sse_upgrade(req, t);
	    agoo_res_message_push(req->res, t);
	    agoo_queue_wakeup(&agoo_server.con_queue);
	    return Qfalse;
	default:
	    break;
	}
    }
    if (AGOO_HEAD == req->method) {
	bsize = 0;
    } else {
	if (T_HASH == rb_type(hv)) {
	    rb_hash_foreach(hv, header_cb, (VALUE)&t);
	} else {
	    rb_iterate(rb_each, hv, header_each_cb, (VALUE)&t);
	}
    }
    t = agoo_text_append(t, "\r\n", 2);
    if (0 < bsize) {
	if (T_ARRAY == rb_type(bv)) {
	    VALUE	v;
	    int		i;
	    int		bcnt = (int)RARRAY_LEN(bv);

	    for (i = 0; i < bcnt; i++) {
		v = rb_ary_entry(bv, i);
		t = agoo_text_append(t, StringValuePtr(v), (int)RSTRING_LEN(v));
	    }
	} else {
	    rb_iterate(rb_each, bv, body_append_cb, (VALUE)&t);
	}
    }
    agoo_res_message_push(req->res, t);
    agoo_queue_wakeup(&agoo_server.con_queue);

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
    agooReq		req = (agooReq)x;
    volatile VALUE	rr = request_wrap(req);
    volatile VALUE	rres = response_new();

    if (NULL != req->hook) {
	rb_funcall((VALUE)req->hook->handler, on_request_id, 2, rr, rres);
    }
    DATA_PTR(rr) = NULL;
    agoo_res_message_push(req->res, response_text(rres));
    agoo_queue_wakeup(&agoo_server.con_queue);

    return Qfalse;
}

static void*
handle_wab(void *x) {
    rb_rescue2(handle_wab_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);

    return NULL;
}

static VALUE
handle_push_inner(void *x) {
    agooReq	req = (agooReq)x;

    switch (req->method) {
    case AGOO_ON_MSG:
	if (req->up->on_msg && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_message_id, 2, (VALUE)req->up->wrap, rb_str_new(req->msg, req->mlen));
	}
	break;
    case AGOO_ON_BIN:
	if (req->up->on_msg && NULL != req->hook) {
	    volatile VALUE	rstr = rb_str_new(req->msg, req->mlen);

	    rb_enc_associate(rstr, rb_ascii8bit_encoding());
	    rb_funcall((VALUE)req->hook->handler, on_message_id, 2, (VALUE)req->up->wrap, rstr);
	}
	break;
    case AGOO_ON_CLOSE:
	agoo_upgraded_ref(req->up);
	agoo_server_publish(agoo_pub_close(req->up));
	if (req->up->on_close && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_close_id, 1, (VALUE)req->up->wrap);
	}
	break;
    case AGOO_ON_SHUTDOWN:
	if (NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, rb_intern("on_shutdown"), 1, (VALUE)req->up->wrap);
	}
	break;
    case AGOO_ON_ERROR:
	if (req->up->on_error && NULL != req->hook) {
	    volatile VALUE	rstr = rb_str_new(req->msg, req->mlen);

	    rb_enc_associate(rstr, rb_ascii8bit_encoding());
	    rb_funcall((VALUE)req->hook->handler, on_error_id, 2, (VALUE)req->up->wrap, rstr);
	}
	break;
    case AGOO_ON_EMPTY:
	if (req->up->on_empty && NULL != req->hook) {
	    rb_funcall((VALUE)req->hook->handler, on_drained_id, 1, (VALUE)req->up->wrap);
	}
	break;
    default:
	break;
    }
    agoo_upgraded_release(req->up);

    return Qfalse;
}

static void*
handle_push(void *x) {
    rb_rescue2(handle_push_inner, (VALUE)x, rescue_error, (VALUE)x, rb_eException, 0);
    return NULL;
}

static void
handle_protected(agooReq req, bool gvi) {
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
    case FUNC_HOOK:
	req->hook->func(req);
	agoo_queue_wakeup(&agoo_server.con_queue);
	break;
    default: {
	char		buf[256];
	int		cnt = snprintf(buf, sizeof(buf), "HTTP/1.1 500 Internal Error\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n");
	agooText	message = agoo_text_create(buf, cnt);

	req->res->close = true;
	agoo_res_message_push(req->res, message);
	agoo_queue_wakeup(&agoo_server.con_queue);
	break;
    }
    }
}

static void*
process_loop(void *ptr) {
    agooReq	req;

    atomic_fetch_add(&agoo_server.running, 1);
    while (agoo_server.active) {
	if (NULL != (req = (agooReq)agoo_queue_pop(&agoo_server.eval_queue, 0.1))) {
	    handle_protected(req, true);
	    agoo_req_destroy(req);
	}
	if (agoo_stop) {
	    agoo_shutdown();
	    break;
	}
    }
    atomic_fetch_sub(&agoo_server.running, 1);

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
    VALUE		*vp;
    int			i;
    int			pid;
    double		giveup;
    struct _agooErr	err = AGOO_ERR_INIT;
    VALUE		agoo = rb_const_get_at(rb_cObject, rb_intern("Agoo"));
    VALUE		v = rb_const_get_at(agoo, rb_intern("VERSION"));

    *the_rserver.worker_pids = getpid();

    // If workers then set the loop_max based on the expected number of
    // threads per worker.
    if (1 < the_rserver.worker_cnt) {
	agoo_server.loop_max /= the_rserver.worker_cnt;
	if (agoo_server.loop_max < 1) {
	    agoo_server.loop_max = 1;
	}
    }
    if (AGOO_ERR_OK != setup_listen(&err)) {
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
	    agoo_log_cat(&agoo_error_cat, "Failed to fork. %s.", strerror(errno));
	    break;
	} else if (0 == pid) {
	    if (AGOO_ERR_OK != agoo_log_start(&err, true)) {
		rb_raise(rb_eStandardError, "%s", err.msg);
	    }
	    break;
	} else {
	    the_rserver.worker_pids[i] = pid;
	}
    }
    if (AGOO_ERR_OK != agoo_server_start(&err, "Agoo", StringValuePtr(v))) {
	rb_raise(rb_eStandardError, "%s", err.msg);
    }
    if (0 >= agoo_server.thread_cnt) {
	agooReq	req;

	while (agoo_server.active) {
	    if (NULL != (req = (agooReq)agoo_queue_pop(&agoo_server.eval_queue, 0.1))) {
		handle_protected(req, false);
		agoo_req_destroy(req);
	    } else {
		rb_thread_schedule();
	    }
	    if (agoo_stop) {
		agoo_shutdown();
		break;
	    }
	}
    } else {
	if (NULL == (the_rserver.eval_threads = (VALUE*)AGOO_MALLOC(sizeof(VALUE) * (agoo_server.thread_cnt + 1)))) {
	    rb_raise(rb_eNoMemError, "Failed to allocate memory for the thread pool.");
	}
	for (i = agoo_server.thread_cnt, vp = the_rserver.eval_threads; 0 < i; i--, vp++) {
	    *vp = rb_thread_create(wrap_process_loop, NULL);
	}
	*vp = Qnil;

	giveup = dtime() + 1.0;
	while (dtime() < giveup) {
	    // The processing threads will not start until this thread
	    // releases ownership so do that and then see if the threads has
	    // been started yet.
	    rb_thread_schedule();
	    if (2 + agoo_server.thread_cnt <= (long)atomic_load(&agoo_server.running)) {
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
	    if (0 >= (long)atomic_load(&agoo_server.running)) {
		break;
	    }
	    dsleep(0.02);
	}
	AGOO_FREE(the_rserver.eval_threads);
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
    if (agoo_server.inited) {
	agoo_server_shutdown("Agoo", stop_runners);

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
			    //printf("exited, status=%d for %d\n", agoo_server.worker_pids[i], WEXITSTATUS(status));
			    the_rserver.worker_pids[i] = 0;
			    exit_cnt++;
			} else if (WIFSIGNALED(status)) {
			    //printf("*** killed by signal %d for %d\n", agoo_server.worker_pids[i], WTERMSIG(status));
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
    agooHook	hook;
    agooMethod	meth = AGOO_ALL;
    const char	*pat;
    ID		static_id = rb_intern("static?");

    rb_check_type(pattern, T_STRING);
    pat = StringValuePtr(pattern);

    if (connect_sym == method) {
	meth = AGOO_CONNECT;
    } else if (delete_sym == method) {
	meth = AGOO_DELETE;
    } else if (get_sym == method) {
	meth = AGOO_GET;
    } else if (head_sym == method) {
	meth = AGOO_HEAD;
    } else if (options_sym == method) {
	meth = AGOO_OPTIONS;
    } else if (post_sym == method) {
	meth = AGOO_POST;
    } else if (put_sym == method) {
	meth = AGOO_PUT;
    } else if (Qnil == method) {
	meth = AGOO_ALL;
    } else {
	rb_raise(rb_eArgError, "invalid method");
    }
    if (T_STRING == rb_type(handler)) {
	handler = resolve_classpath(StringValuePtr(handler), RSTRING_LEN(handler));
    }
    if (rb_respond_to(handler, static_id)) {
	if (Qtrue == rb_funcall(handler, static_id, 0)) {
	    VALUE	res = rb_funcall(handler, call_id, 1, Qnil);
	    VALUE	bv;

	    rb_check_type(res, T_ARRAY);
	    if (3 != RARRAY_LEN(res)) {
		rb_raise(rb_eArgError, "a rack call() response must be an array of 3 members.");
	    }
	    bv = rb_ary_entry(res, 2);
	    if (T_ARRAY == rb_type(bv)) {
		int		i;
		int		bcnt = (int)RARRAY_LEN(bv);
		agooText	t = agoo_text_allocate(1024);
		struct _agooErr	err = AGOO_ERR_INIT;
		VALUE		v;

		if (NULL == t) {
		    rb_raise(rb_eArgError, "failed to allocate response.");
		}
		for (i = 0; i < bcnt; i++) {
		    v = rb_ary_entry(bv, i);
		    t = agoo_text_append(t, StringValuePtr(v), (int)RSTRING_LEN(v));
		}
		if (NULL == t) {
		    rb_raise(rb_eNoMemError, "Failed to allocate memory for a response.");
		}
		if (NULL == agoo_page_immutable(&err, pat, t->text, t->len)) {
		    rb_raise(rb_eArgError, "%s", err.msg);
		}
		agoo_text_release(t);

		return Qnil;
	    }
	}
    }
    if (NULL == (hook = rhook_create(meth, pat, handler, &agoo_server.eval_queue))) {
	rb_raise(rb_eStandardError, "out of memory.");
    } else {
	agooHook	h;
	agooHook	prev = NULL;

	for (h = agoo_server.hooks; NULL != h; h = h->next) {
	    prev = h;
	}
	if (NULL != prev) {
	    prev->next = hook;
	} else {
	    agoo_server.hooks = hook;
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
    if (NULL == (agoo_server.hook404 = rhook_create(AGOO_GET, "/", handler, &agoo_server.eval_queue))) {
	rb_raise(rb_eStandardError, "out of memory.");
    }
    rb_gc_register_address((VALUE*)&agoo_server.hook404->handler);

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
    struct _agooErr	err = AGOO_ERR_INIT;

    if (AGOO_ERR_OK != mime_set(&err, StringValuePtr(suffix), StringValuePtr(type))) {
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
    struct _agooErr	err = AGOO_ERR_INIT;
    agooGroup		g;

    rb_check_type(path, T_STRING);
    rb_check_type(dirs, T_ARRAY);

    if (NULL != (g = agoo_group_create(StringValuePtr(path)))) {
	int	i;
	int	dcnt = (int)RARRAY_LEN(dirs);
	VALUE	entry;

	for (i = dcnt - 1; 0 <= i; i--) {
	    entry = rb_ary_entry(dirs, i);
	    if (T_STRING != rb_type(entry)) {
		entry = rb_funcall(entry, rb_intern("to_s"), 0);
	    }
	    if (NULL == agoo_group_add(&err, g, StringValuePtr(entry))) {
		rb_raise(rb_eStandardError, "%s", err.msg);
	    }
	}
    }
    return Qnil;
}

/* Document-method: header_rule
 *
 * call-seq: header_rule(path, mime, key, value)
 *
 * Add a header rule. A header rule will add the key and value to the headers
 * of any static asset that matches the path and mime type specified. The path
 * pattern follows glob like rules in that a single * matches a single token
 * bounded by the `/` character and a double ** matches all remaining. The
 * mime can also be a * which matches all types. The mime argument will be
 * compared to the mine type as well as the file extension so
 * 'applicaiton/json', a mime type can be used as can 'json' as a file
 * extension. All rules that match add the header key and value to the header
 * of a static asset.
 *
 * Note that the server must be initialized before calling this method.
 */
static VALUE
header_rule(VALUE self, VALUE path, VALUE mime, VALUE key, VALUE value) {
    struct _agooErr	err = AGOO_ERR_INIT;

    rb_check_type(path, T_STRING);
    rb_check_type(mime, T_STRING);
    rb_check_type(key, T_STRING);
    rb_check_type(value, T_STRING);

    if (AGOO_ERR_OK != agoo_header_rule(&err, StringValuePtr(path), StringValuePtr(mime), StringValuePtr(key), StringValuePtr(value))) {
	rb_raise(rb_eArgError, "%s", err.msg);
    }
    return Qnil;
}

/* Document-method: domain
 *
 * call-seq: domain(host, path)
 *
 * Sets up a sub-domain. The first argument, _host_ should be either a String
 * or a Regexp that includes variable replacement elements. The _path_
 * argument should also be a string. If the _host_ argument is a Regex then
 * the $(n) sequence will be replaced by the matching variable in the Regex
 * result. The _path_ is the root of the sub-domain.
 */
static VALUE
domain(VALUE self, VALUE host, VALUE path) {
    struct _agooErr	err = AGOO_ERR_INIT;

    switch(rb_type(host)) {
    case RUBY_T_STRING:
	rb_check_type(path, T_STRING);
	if (AGOO_ERR_OK != agoo_domain_add(&err, rb_string_value_ptr((VALUE*)&host), rb_string_value_ptr((VALUE*)&path))) {
	    rb_raise(rb_eArgError, "%s", err.msg);
	}
	break;
    case RUBY_T_REGEXP: {
	volatile VALUE	v = rb_funcall(host, rb_intern("inspect"), 0);
	char		rx[1024];

	if (sizeof(rx) <= (size_t)RSTRING_LEN(v)) {
	    rb_raise(rb_eArgError, "host Regex limited to %ld characters", sizeof(rx));
	}
	strcpy(rx, rb_string_value_ptr((VALUE*)&v) + 1);
	rx[RSTRING_LEN(v) - 2] = '\0';
	if (AGOO_ERR_OK != agoo_domain_add_regex(&err, rx, rb_string_value_ptr((VALUE*)&path))) {
	    rb_raise(rb_eArgError, "%s", err.msg);
	}
	break;
    }
    default:
	rb_raise(rb_eArgError, "host must be a String or Regex");
	break;
    }
    return Qnil;
}

/* Document-method: rack_early_hints
 *
 * call-seq: rack_early_hints(on)
 *
 * Turns on or off the inclusion of a early_hints object in the rack call env
 * Hash. If the argument is nil then the current value is returned.
 */
static VALUE
rack_early_hints(VALUE self, VALUE on) {
    if (Qtrue == on) {
	agoo_server.rack_early_hints = true;
    } else if (Qfalse == on) {
	agoo_server.rack_early_hints = false;
    } else if (Qnil == on) {
	on = agoo_server.rack_early_hints ? Qtrue : Qfalse;
    } else {
	rb_raise(rb_eArgError, "rack_early_hints can only be set to true or false");
    }
    return on;
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
    rb_define_module_function(server_mod, "header_rule", header_rule, 4);
    rb_define_module_function(server_mod, "domain", domain, 2);

    rb_define_module_function(server_mod, "rack_early_hints", rack_early_hints, 1);

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

    agoo_http_init();
}
