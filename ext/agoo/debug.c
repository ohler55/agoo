// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdio.h>

#include <ruby.h>

#include "debug.h"

atomic_int	mem_con = 0;
atomic_int	mem_cslot = 0;
atomic_int	mem_err_stream = 0;
atomic_int	mem_eval_threads = 0;
atomic_int	mem_header = 0;
atomic_int	mem_hook = 0;
atomic_int	mem_hook_pattern = 0;
atomic_int	mem_http_slot = 0;
atomic_int	mem_log_entry = 0;
atomic_int	mem_log_what = 0;
atomic_int	mem_mime_slot = 0;
atomic_int	mem_page = 0;
atomic_int	mem_page_msg = 0;
atomic_int	mem_page_path = 0;
atomic_int	mem_page_slot = 0;
atomic_int	mem_pub = 0;
atomic_int	mem_qitem = 0;
atomic_int	mem_queue_item = 0;
atomic_int	mem_rack_logger = 0;
atomic_int	mem_req = 0;
atomic_int	mem_res = 0;
atomic_int	mem_res_body = 0;
atomic_int	mem_response = 0;
atomic_int	mem_server = 0;
atomic_int	mem_text = 0;
atomic_int	mem_to_s = 0;

void
debug_print_stats() {
#ifdef MEM_DEBUG
    rb_gc_enable();
    rb_gc();
    
    printf("********************************************************************************\n");
    printf("memory statistics\n");
    printf("  mem_con:          %d\n", mem_con);
    printf("  mem_cslot:        %d\n", mem_cslot);
    printf("  mem_err_stream:   %d\n", mem_err_stream);
    printf("  mem_eval_threads: %d\n", mem_eval_threads);
    printf("  mem_header:       %d\n", mem_header);
    printf("  mem_hook:         %d\n", mem_hook);
    printf("  mem_hook_pattern: %d\n", mem_hook_pattern);
    printf("  mem_http_slot:    %d\n", mem_http_slot);
    printf("  mem_log_entry:    %d\n", mem_log_entry);
    printf("  mem_log_what:     %d\n", mem_log_what);
    printf("  mem_mime_slot:    %d\n", mem_mime_slot);
    printf("  mem_page:         %d\n", mem_page);
    printf("  mem_page_msg:     %d\n", mem_page_msg);
    printf("  mem_page_path:    %d\n", mem_page_path);
    printf("  mem_page_slot:    %d\n", mem_page_slot);
    printf("  mem_pub:          %d\n", mem_pub);
    printf("  mem_qitem:        %d\n", mem_qitem);
    printf("  mem_queue_item:   %d\n", mem_queue_item);
    printf("  mem_rack_logger:  %d\n", mem_rack_logger);
    printf("  mem_req:          %d\n", mem_req);
    printf("  mem_res:          %d\n", mem_res);
    printf("  mem_res_body:     %d\n", mem_res_body);
    printf("  mem_response:     %d\n", mem_response);
    printf("  mem_server:       %d\n", mem_server);
    printf("  mem_text:         %d\n", mem_text);
    printf("  mem_to_s:         %d\n", mem_to_s);
#endif
}
