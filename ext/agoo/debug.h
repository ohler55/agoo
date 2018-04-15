// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_DEBUG_H__
#define __AGOO_DEBUG_H__

#include <stdatomic.h>

#ifdef MEM_DEBUG
#define DEBUG_ALLOC(var) { atomic_fetch_add(&var, 1); }
#define DEBUG_FREE(var) { atomic_fetch_sub(&var, 1); }
#else
#define DEBUG_ALLOC(var) { }
#define DEBUG_FREE(var) { }
#endif

extern atomic_int	mem_con;
extern atomic_int	mem_cslot;
extern atomic_int	mem_err_stream;
extern atomic_int	mem_eval_threads;
extern atomic_int	mem_header;
extern atomic_int	mem_hook;
extern atomic_int	mem_hook_pattern;
extern atomic_int	mem_http_slot;
extern atomic_int	mem_log_entry;
extern atomic_int	mem_log_what;
extern atomic_int	mem_mime_slot;
extern atomic_int	mem_page;
extern atomic_int	mem_page_msg;
extern atomic_int	mem_page_path;
extern atomic_int	mem_page_slot;
extern atomic_int	mem_qitem;
extern atomic_int	mem_queue_item;
extern atomic_int	mem_rack_logger;
extern atomic_int	mem_req;
extern atomic_int	mem_res;
extern atomic_int	mem_res_body;
extern atomic_int	mem_response;
extern atomic_int	mem_server;
extern atomic_int	mem_text;
extern atomic_int	mem_to_s;

extern void	debug_print_stats();

#endif /* __AGOO_DEBUG_H__ */
