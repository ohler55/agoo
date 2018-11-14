// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_DEBUG_H
#define AGOO_DEBUG_H

#include "atomic.h"

#ifdef MEM_DEBUG
#define DEBUG_ALLOC(var, ptr) { atomic_fetch_add(&var, 1); debug_add(ptr, #var, FILE, LINE); }
#define DEBUG_REALLOC(var, orig, ptr) { debug_update(orig, ptr, #var, FILE, LINE); }
#define DEBUG_FREE(var, ptr) { atomic_fetch_sub(&var, 1); debug_del(ptr, FILE, LINE); }
#else
#define DEBUG_ALLOC(var, ptr) { }
#define DEBUG_REALLOC(var, orig, ptr) { }
#define DEBUG_FREE(var, ptr) { }
#endif

extern atomic_int	mem_bind;
extern atomic_int	mem_cb;
extern atomic_int	mem_con;
extern atomic_int	mem_cslot;
extern atomic_int	mem_err_stream;
extern atomic_int	mem_eval_threads;
extern atomic_int	mem_graphql_arg;
extern atomic_int	mem_graphql_directive;
extern atomic_int	mem_graphql_field;
extern atomic_int	mem_graphql_link;
extern atomic_int	mem_graphql_slot;
extern atomic_int	mem_graphql_type;
extern atomic_int	mem_graphql_value;
extern atomic_int	mem_group;
extern atomic_int	mem_group_path;
extern atomic_int	mem_header;
extern atomic_int	mem_hook;
extern atomic_int	mem_hook_pattern;
extern atomic_int	mem_http_slot;
extern atomic_int	mem_log_entry;
extern atomic_int	mem_log_what;
extern atomic_int	mem_log_tid;
extern atomic_int	mem_mime_slot;
extern atomic_int	mem_page;
extern atomic_int	mem_page_msg;
extern atomic_int	mem_page_path;
extern atomic_int	mem_page_slot;
extern atomic_int	mem_pub;
extern atomic_int	mem_qitem;
extern atomic_int	mem_queue_item;
extern atomic_int	mem_rack_logger;
extern atomic_int	mem_req;
extern atomic_int	mem_res;
extern atomic_int	mem_res_body;
extern atomic_int	mem_response;
extern atomic_int	mem_subject;
extern atomic_int	mem_text;
extern atomic_int	mem_to_s;
extern atomic_int	mem_upgraded;

extern void	debug_add(void *ptr, const char *type, const char *file, int line);
extern void	debug_update(void *orig, void *ptr, const char *type, const char *file, int line);
extern void	debug_del(void *ptr, const char *file, int line);
extern void	debug_report();
extern void	debug_rreport(); // when called from ruby

#endif /* AGOO_DEBUG_H */
