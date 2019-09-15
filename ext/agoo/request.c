// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "debug.h"
#include "con.h"
#include "early_hints.h"
#include "error_stream.h"
#include "rack_logger.h"
#include "request.h"
#include "res.h"

static VALUE	req_class = Qundef;

static VALUE	connect_val = Qundef;
static VALUE	content_length_val = Qundef;
static VALUE	content_type_val = Qundef;
static VALUE	delete_val = Qundef;
static VALUE	early_hints_val = Qundef;
static VALUE	empty_val = Qundef;
static VALUE	get_val = Qundef;
static VALUE	head_val = Qundef;
static VALUE	http_val = Qundef;
static VALUE	https_val = Qundef;
static VALUE	options_val = Qundef;
static VALUE	patch_val = Qundef;
static VALUE	path_info_val = Qundef;
static VALUE	post_val = Qundef;
static VALUE	put_val = Qundef;
static VALUE	query_string_val = Qundef;
static VALUE	rack_errors_val = Qundef;
static VALUE	rack_hijack_io_val = Qundef;
static VALUE	rack_hijack_val = Qundef;
static VALUE	rack_hijackq_val = Qundef;
static VALUE	rack_input_val = Qundef;
static VALUE	rack_logger_val = Qundef;
static VALUE	rack_multiprocess_val = Qundef;
static VALUE	rack_multithread_val = Qundef;
static VALUE	rack_run_once_val = Qundef;
static VALUE	rack_upgrade_val = Qundef;
static VALUE	rack_url_scheme_val = Qundef;
static VALUE	rack_version_val = Qundef;
static VALUE	rack_version_val_val = Qundef;
static VALUE	request_method_val = Qundef;
static VALUE	script_name_val = Qundef;
static VALUE	server_name_val = Qundef;
static VALUE	server_port_val = Qundef;
static VALUE	slash_val = Qundef;

static VALUE	sse_sym;
static VALUE	websocket_sym;

static VALUE	stringio_class = Qundef;

static ID	new_id;

static const char	content_type[] = "Content-Type";
static const char	content_length[] = "Content-Length";
static const char	connection_key[] = "Connection";
static const char	upgrade_key[] = "Upgrade";
static const char	websocket_val[] = "websocket";
static const char	accept_key[] = "Accept";
static const char	event_stream_val[] = "text/event-stream";

static VALUE
req_method(agooReq r) {
    VALUE	m;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    switch (r->method) {
    case AGOO_CONNECT:	m = connect_val;	break;
    case AGOO_DELETE:	m = delete_val;		break;
    case AGOO_GET:	m = get_val;		break;
    case AGOO_HEAD:	m = head_val;		break;
    case AGOO_OPTIONS:	m = options_val;	break;
    case AGOO_POST:	m = post_val;		break;
    case AGOO_PUT:	m = put_val;		break;
    case AGOO_PATCH:	m = patch_val;		break;
    default:		m = Qnil;		break;
    }
    return m;
}

/* Document-method: request_method
 *
 * call-seq: request_method()
 *
 * Returns the HTTP method of the request.
 */
static VALUE
method(VALUE self) {
    return req_method((agooReq)DATA_PTR(self));
}

static VALUE
req_script_name(agooReq r) {
    // The logic is a bit tricky here and for path_info. If the HTTP path is /
    // then the script_name must be empty and the path_info will be /. All
    // other cases are handled with the full path in script_name and path_info
    // empty.
    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    return empty_val;
}

/* Document-method: script_name
 *
 * call-seq: script_name()
 *
 * Returns the path info which is assumed to be the full path unless the root
 * and then the rack restrictions are followed on what the script name and
 * path info should be.
 */
static VALUE
script_name(VALUE self) {
    return req_script_name((agooReq)DATA_PTR(self));
}

static VALUE
req_path_info(agooReq r) {
    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (0 == r->path.len || (1 == r->path.len && '/' == *r->path.start)) {
	return slash_val;
    }

    if (0 == r->path.len || (1 == r->path.len && '/' == *r->path.start)) {
	return empty_val;
    }
    return rb_str_new(r->path.start, r->path.len);
}

/* Document-method: path_info
 *
 * call-seq: path_info()
 *
 * Returns the script name which is assumed to be either '/' or the empty
 * according to the rack restrictions are followed on what the script name and
 * path info should be.
 */
static VALUE
path_info(VALUE self) {
    return req_path_info((agooReq)DATA_PTR(self));
}

static VALUE
req_query_string(agooReq r) {
    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (NULL == r->query.start) {
	return empty_val;
    }
    return rb_str_new(r->query.start, r->query.len);
}

/* Document-method: query_string
 *
 * call-seq: query_string()
 *
 * Returns the query string of the request.
 */
static VALUE
query_string(VALUE self) {
    return req_query_string((agooReq)DATA_PTR(self));
}

static VALUE
req_server_name(agooReq r) {
    int		len;
    const char	*host;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (NULL == (host = agoo_req_host(r, &len))) {
	return rb_str_new2("unknown");
    }
    return rb_str_new(host, len);
}

/* Document-method: server_name
 *
 * call-seq: server_name()
 *
 * Returns the server or host name.
 */
static VALUE
server_name(VALUE self) {
    return req_server_name((agooReq)DATA_PTR(self));
}

static VALUE
req_server_port(agooReq r) {
    int		len;
    const char	*host;
    const char	*colon;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (NULL == (host = agoo_con_header_value(r->header.start, r->header.len, "Host", &len))) {
	return Qnil;
    }
    for (colon = host + len - 1; host < colon; colon--) {
	if (':' == *colon) {
	    break;
	}
    }
    if (host == colon) {
	return Qnil;
    }
    return rb_str_new(colon + 1, host + len - colon - 1);
}

/* Document-method: server_port
 *
 * call-seq: server_port()
 *
 * Returns the server or host port as a string.
 */
static VALUE
server_port(VALUE self) {
    return req_server_port((agooReq)DATA_PTR(self));
}

static VALUE
req_rack_upgrade(agooReq r) {
    switch (r->upgrade) {
    case AGOO_UP_WS:	return websocket_sym;
    case AGOO_UP_SSE:	return sse_sym;
    default:		return Qnil;
    }
}

/* Document-method: rack_upgrade?
 *
 * call-seq: rack_upgrade?()
 *
 * Returns the URL scheme or either _http_ or _https_ as a string.
 */
static VALUE
rack_upgrade(VALUE self) {
    return req_rack_upgrade((agooReq)DATA_PTR(self));
}

/* Document-method: rack_version
 *
 * call-seq: rack_version()
 *
 * Returns the rack version the request is compliant with.
 */
static VALUE
rack_version(VALUE self) {
    return rack_version_val_val;
}

static VALUE
req_rack_url_scheme(agooReq r) {
    if (AGOO_CON_HTTPS == r->res->con->bind->kind) {
	return https_val;
    }
    return http_val;
}

/* Document-method: rack_url_scheme
 *
 * call-seq: rack_url_scheme()
 *
 * Returns the URL scheme or either _http_ or _https_ as a string.
 */
static VALUE
rack_url_scheme(VALUE self) {
    return req_rack_url_scheme((agooReq)DATA_PTR(self));
}

static VALUE
req_rack_input(agooReq r) {
    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (NULL == r->body.start) {
	return Qnil;
    }
    return rb_funcall(stringio_class, new_id, 1, rb_str_new(r->body.start, r->body.len));
}

/* Document-method: rack_input
 *
 * call-seq: rack_input()
 *
 * Returns an input stream for the request body. If no body is present then
 * _nil_ is returned.
 */
static VALUE
rack_input(VALUE self) {
    return req_rack_input((agooReq)DATA_PTR(self));
}

static VALUE
req_rack_errors(agooReq r) {
    return error_stream_new();
}

/* Document-method: rack_errors
 *
 * call-seq: rack_errors()
 *
 * Returns an error stream for the request. This stream is used to write error
 * log entries.
 */
static VALUE
rack_errors(VALUE self) {
    return req_rack_errors((agooReq)DATA_PTR(self));
}

static VALUE
req_rack_multithread(agooReq r) {
    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (1 < agoo_server.thread_cnt) {
	return Qtrue;
    }
    return Qfalse;
}

/* Document-method: rack_multithread
 *
 * call-seq: rack_multithread()
 *
 * Returns true is the server is using multiple handler worker threads.
 */
static VALUE
rack_multithread(VALUE self) {
    return req_rack_multithread((agooReq)DATA_PTR(self));
}

/* Document-method: rack_multiprocess
 *
 * call-seq: rack_multiprocess()
 *
 * Returns false since the server is a single process.
 */
static VALUE
rack_multiprocess(VALUE self) {
    return Qfalse;
}

/* Document-method: rack_run_once
 *
 * call-seq: rack_run_once()
 *
 * Returns false.
 */
static VALUE
rack_run_once(VALUE self) {
    return Qfalse;
}

static void
add_header_value(VALUE hh, const char *key, int klen, const char *val, int vlen) {
    if (sizeof(content_type) - 1 == klen && 0 == strncasecmp(key, content_type, sizeof(content_type) - 1)) {
	rb_hash_aset(hh, content_type_val, rb_str_new(val, vlen));
    } else if (sizeof(content_length) - 1 == klen && 0 == strncasecmp(key, content_length, sizeof(content_length) - 1)) {
	rb_hash_aset(hh, content_length_val, rb_str_new(val, vlen));
    } else {
	char		hkey[1024];
	char		*k = hkey;
	volatile VALUE	sval = rb_str_new(val, vlen);

	strcpy(hkey, "HTTP_");
	k = hkey + 5;
	if ((int)(sizeof(hkey) - 5) <= klen) {
	    klen = sizeof(hkey) - 6;
	}
	strncpy(k, key, klen);
	hkey[klen + 5] = '\0';

	//rb_hash_aset(hh, rb_str_new(hkey, klen + 5), sval);
	// Contrary to the Rack spec, Rails expects all upper case keys so add those as well.
	for (k = hkey + 5; '\0' != *k; k++) {
	    if ('-' == *k) {
		*k = '_';
	    } else {
		*k = toupper(*k);
	    }
	}
	rb_hash_aset(hh, rb_str_new(hkey, klen + 5), sval);
    }
}

static void
fill_headers(agooReq r, VALUE hash) {
    char	*h = r->header.start;
    char	*end = h + r->header.len + 1; // +1 for last \r
    char	*key = h;
    char	*kend = key;
    char	*val = NULL;
    char	*vend;
    int		klen;
    bool	upgrade = false;
    bool	ws = false;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }

    for (; h < end; h++) {
	switch (*h) {
	case ':':
	    if (NULL == val) {
		kend = h;
		val = h + 1;
	    }
	    break;
	case '\r':
	    if (NULL != val) {
		for (; ' ' == *val; val++) {
		}
		vend = h;
	    }
	    if (NULL != key) {
		for (; ' ' == *key; key++) {
		}
	    }
	    if ('\n' == *(h + 1)) {
		h++;
	    }
	    klen = (int)(kend - key);
	    add_header_value(hash, key, klen, val, (int)(vend - val));
	    if (sizeof(upgrade_key) - 1 == klen && 0 == strncasecmp(key, upgrade_key, sizeof(upgrade_key) - 1)) {
		if (sizeof(websocket_val) - 1 == vend - val &&
		    0 == strncasecmp(val, websocket_val, sizeof(websocket_val) - 1)) {
		    ws = true;
		}
	    } else if (sizeof(connection_key) - 1 == klen && 0 == strncasecmp(key, connection_key, sizeof(connection_key) - 1)) {
		char	buf[1024];

		strncpy(buf, val, vend - val);
		buf[sizeof(buf)-1] = '\0';
		if (NULL != strstr(buf, upgrade_key)) {
		    upgrade = true;
		}
	    } else if (sizeof(accept_key) - 1 == klen && 0 == strncasecmp(key, accept_key, sizeof(accept_key) - 1)) {
		if (sizeof(event_stream_val) - 1 == vend - val &&
		    0 == strncasecmp(val, event_stream_val, sizeof(event_stream_val) - 1)) {
		    r->upgrade = AGOO_UP_SSE;
		}
	    }
	    key = h + 1;
	    kend = NULL;
	    val = NULL;
	    vend = NULL;
	    break;
	default:
	    break;
	}
    }
    if (upgrade && ws) {
	r->upgrade = AGOO_UP_WS;
    }
}

/* Document-method: headers
 *
 * call-seq: headers()
 *
 * Returns the header of the request as a Hash.
 */
static VALUE
headers(VALUE self) {
    agooReq		r = DATA_PTR(self);
    volatile VALUE	h;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    h = rb_hash_new();
    fill_headers(r, h);

    return h;
}

/* Document-method: body
 *
 * call-seq: body()
 *
 * Returns the body of the request as a String. If there is no body then _nil_
 * is returned.
 */
static VALUE
body(VALUE self) {
    agooReq	r = DATA_PTR(self);

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    if (NULL == r->body.start) {
	return Qnil;
    }
    return rb_str_new(r->body.start, r->body.len);
}

static VALUE
req_rack_logger(agooReq req) {
    return rack_logger_new();
}

/* Document-method: rack_logger
 *
 * call-seq: rack_logger()
 *
 * Returns a RackLogger that can be used to log messages with the server
 * logger. The methods supported are debug(), info(), warn(), error(), and
 * fatal(). The signature is the same as the standard Ruby Logger of
 * info(message, &block).
 */
static VALUE
rack_logger(VALUE self) {
    return req_rack_logger((agooReq)DATA_PTR(self));
}

/* Document-class: Agoo::Request
 *
 * A Request is passes to handler that respond to the _on_request_ method. The
 * request is a more efficient encapsulation of the rack environment.
 */
VALUE
request_env(agooReq req, VALUE self) {
    if (Qnil == (VALUE)req->env) {
	volatile VALUE	env = rb_hash_new();

	// As described by
	// http://www.rubydoc.info/github/rack/rack/master/file/SPEC and
	// https://github.com/rack/rack/blob/master/SPEC.

	rb_hash_aset(env, request_method_val, req_method(req));
	rb_hash_aset(env, script_name_val, req_script_name(req));
	rb_hash_aset(env, path_info_val, req_path_info(req));
	rb_hash_aset(env, query_string_val, req_query_string(req));
	rb_hash_aset(env, server_name_val, req_server_name(req));
	rb_hash_aset(env, server_port_val, req_server_port(req));
	fill_headers(req, env);
	rb_hash_aset(env, rack_version_val, rack_version_val_val);
	rb_hash_aset(env, rack_url_scheme_val, req_rack_url_scheme(req));
	rb_hash_aset(env, rack_input_val, req_rack_input(req));
	rb_hash_aset(env, rack_errors_val, req_rack_errors(req));
	rb_hash_aset(env, rack_multithread_val, req_rack_multithread(req));
	rb_hash_aset(env, rack_multiprocess_val, Qfalse);
	rb_hash_aset(env, rack_run_once_val, Qfalse);
	rb_hash_aset(env, rack_logger_val, req_rack_logger(req));
	rb_hash_aset(env, rack_upgrade_val, req_rack_upgrade(req));
	rb_hash_aset(env, rack_hijackq_val, Qtrue);

	// TBD should return IO on #call and set hijack_io on env object that
	//  has a call method that wraps the req->res->con->sock then set the
	//  sock to 0 or maybe con. mutex? env[rack.hijack_io] = IO.new(sock,
	//  "rw") - maybe it works.
	//
	// set a flag on con to indicate it has been hijacked
	// then set sock to 0 in con loop and destroy con
	rb_hash_aset(env, rack_hijack_val, self);
	rb_hash_aset(env, rack_hijack_io_val, Qnil);

	if (agoo_server.rack_early_hints) {
	    volatile VALUE	eh = agoo_early_hints_new(req);

	    rb_hash_aset(env, early_hints_val, eh);
	}
	req->env = (void*)env;
    }
    return (VALUE)req->env;
}

/* Document-method: to_h
 *
 * call-seq: to_h()
 *
 * Returns a Hash representation of the request which is the same as a rack
 * environment Hash.
 */
static VALUE
to_h(VALUE self) {
    agooReq	r = DATA_PTR(self);

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    return request_env(r, self);
}

/* Document-method: to_s
 *
 * call-seq: to_s()
 *
 * Returns a string representation of the request.
 */
static VALUE
to_s(VALUE self) {
    volatile VALUE	h = to_h(self);

    return rb_funcall(h, rb_intern("to_s"), 0);
}

/* Document-method: call
 *
 * call-seq: call()
 *
 * Returns an IO like object and hijack the connection.
 */
static VALUE
call(VALUE self) {
    agooReq		r = DATA_PTR(self);
    VALUE		args[1];
    volatile VALUE	io;

    if (NULL == r) {
	rb_raise(rb_eArgError, "Request is no longer valid.");
    }
    r->res->con->hijacked = true;

    // TBD try basic IO first. If that fails define a socket class
    // is a mode needed?

    args[0] = INT2NUM(r->res->con->sock);

    io = rb_class_new_instance(1, args, rb_cIO);
    rb_hash_aset((VALUE)r->env, rack_hijack_io_val, io);

    return io;
}

VALUE
request_wrap(agooReq req) {
    // freed from the C side of things
    return Data_Wrap_Struct(req_class, NULL, NULL, req);
}

/* Document-class: Agoo::Request
 *
 * A representation of an HTTP request that is used with a handler that
 * responds to the _on_request_ method. The request is a more efficient
 * encapsulation of the rack environment.
 */
void
request_init(VALUE mod) {
    req_class = rb_define_class_under(mod, "Request", rb_cObject);

    rb_define_method(req_class, "to_s", to_s, 0);
    rb_define_method(req_class, "to_h", to_h, 0);
    rb_define_method(req_class, "environment", to_h, 0);
    rb_define_method(req_class, "env", to_h, 0);
    rb_define_method(req_class, "request_method", method, 0);
    rb_define_method(req_class, "script_name", script_name, 0);
    rb_define_method(req_class, "path_info", path_info, 0);
    rb_define_method(req_class, "query_string", query_string, 0);
    rb_define_method(req_class, "server_name", server_name, 0);
    rb_define_method(req_class, "server_port", server_port, 0);
    rb_define_method(req_class, "rack_version", rack_version, 0);
    rb_define_method(req_class, "rack_url_scheme", rack_url_scheme, 0);
    rb_define_method(req_class, "rack_input", rack_input, 0);
    rb_define_method(req_class, "rack_errors", rack_errors, 0);
    rb_define_method(req_class, "rack_multithread", rack_multithread, 0);
    rb_define_method(req_class, "rack_multiprocess", rack_multiprocess, 0);
    rb_define_method(req_class, "rack_run_once", rack_run_once, 0);
    rb_define_method(req_class, "rack_upgrade?", rack_upgrade, 0);
    rb_define_method(req_class, "headers", headers, 0);
    rb_define_method(req_class, "body", body, 0);
    rb_define_method(req_class, "rack_logger", rack_logger, 0);
    rb_define_method(req_class, "call", call, 0);

    new_id = rb_intern("new");

    rack_version_val_val = rb_ary_new();
    rb_ary_push(rack_version_val_val, INT2NUM(1));
    rb_ary_push(rack_version_val_val, INT2NUM(3));
    rb_gc_register_address(&rack_version_val_val);

    stringio_class = rb_const_get(rb_cObject, rb_intern("StringIO"));

    connect_val = rb_str_new_cstr("CONNECT");			rb_gc_register_address(&connect_val);
    content_length_val = rb_str_new_cstr("CONTENT_LENGTH");	rb_gc_register_address(&content_length_val);
    content_type_val = rb_str_new_cstr("CONTENT_TYPE");		rb_gc_register_address(&content_type_val);
    delete_val = rb_str_new_cstr("DELETE");			rb_gc_register_address(&delete_val);
    early_hints_val = rb_str_new_cstr("early_hints");		rb_gc_register_address(&early_hints_val);
    empty_val = rb_str_new_cstr("");				rb_gc_register_address(&empty_val);
    get_val = rb_str_new_cstr("GET");				rb_gc_register_address(&get_val);
    head_val = rb_str_new_cstr("HEAD");				rb_gc_register_address(&head_val);
    http_val = rb_str_new_cstr("http");				rb_gc_register_address(&http_val);
    https_val = rb_str_new_cstr("https");			rb_gc_register_address(&https_val);
    options_val = rb_str_new_cstr("OPTIONS");			rb_gc_register_address(&options_val);
    patch_val = rb_str_new_cstr("PATCH");			rb_gc_register_address(&patch_val);
    path_info_val = rb_str_new_cstr("PATH_INFO");		rb_gc_register_address(&path_info_val);
    post_val = rb_str_new_cstr("POST");				rb_gc_register_address(&post_val);
    put_val = rb_str_new_cstr("PUT");				rb_gc_register_address(&put_val);
    query_string_val = rb_str_new_cstr("QUERY_STRING");		rb_gc_register_address(&query_string_val);
    rack_errors_val = rb_str_new_cstr("rack.errors");		rb_gc_register_address(&rack_errors_val);
    rack_hijack_io_val = rb_str_new_cstr("rack.hijack_io");	rb_gc_register_address(&rack_hijack_io_val);
    rack_hijack_val = rb_str_new_cstr("rack.hijack");		rb_gc_register_address(&rack_hijack_val);
    rack_hijackq_val = rb_str_new_cstr("rack.hijack?");		rb_gc_register_address(&rack_hijackq_val);
    rack_input_val = rb_str_new_cstr("rack.input");		rb_gc_register_address(&rack_input_val);
    rack_logger_val = rb_str_new_cstr("rack.logger");		rb_gc_register_address(&rack_logger_val);
    rack_multiprocess_val = rb_str_new_cstr("rack.multiprocess");rb_gc_register_address(&rack_multiprocess_val);
    rack_multithread_val = rb_str_new_cstr("rack.multithread");	rb_gc_register_address(&rack_multithread_val);
    rack_run_once_val = rb_str_new_cstr("rack.run_once");	rb_gc_register_address(&rack_run_once_val);
    rack_upgrade_val = rb_str_new_cstr("rack.upgrade?");	rb_gc_register_address(&rack_upgrade_val);
    rack_url_scheme_val = rb_str_new_cstr("rack.url_scheme");	rb_gc_register_address(&rack_url_scheme_val);
    rack_version_val = rb_str_new_cstr("rack.version");		rb_gc_register_address(&rack_version_val);
    request_method_val = rb_str_new_cstr("REQUEST_METHOD");	rb_gc_register_address(&request_method_val);
    script_name_val = rb_str_new_cstr("SCRIPT_NAME");		rb_gc_register_address(&script_name_val);
    server_name_val = rb_str_new_cstr("SERVER_NAME");		rb_gc_register_address(&server_name_val);
    server_port_val = rb_str_new_cstr("SERVER_PORT");		rb_gc_register_address(&server_port_val);
    slash_val = rb_str_new_cstr("/");				rb_gc_register_address(&slash_val);

    sse_sym = ID2SYM(rb_intern("sse"));				rb_gc_register_address(&sse_sym);
    websocket_sym = ID2SYM(rb_intern("websocket"));		rb_gc_register_address(&websocket_sym);
}
