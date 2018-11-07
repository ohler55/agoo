// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

//#include <ruby.h>
//#include <ruby/thread.h>

#include "debug.h"

typedef struct _Rec {
    struct _Rec	*next;
    void	*ptr;
    const char	*type;
    const char	*file;
    int		line;
} *Rec;

static pthread_mutex_t	lock = PTHREAD_MUTEX_INITIALIZER;
static Rec		recs = NULL;

atomic_int	mem_bind = 0;
atomic_int	mem_cb = 0;
atomic_int	mem_con = 0;
atomic_int	mem_cslot = 0;
atomic_int	mem_err_stream = 0;
atomic_int	mem_eval_threads = 0;
atomic_int	mem_group = 0;
atomic_int	mem_graphql_arg = 0;
atomic_int	mem_graphql_directive = 0;
atomic_int	mem_graphql_field = 0;
atomic_int	mem_graphql_link = 0;
atomic_int	mem_graphql_slot = 0;
atomic_int	mem_graphql_type = 0;
atomic_int	mem_graphql_value = 0;
atomic_int	mem_group_path = 0;
atomic_int	mem_header = 0;
atomic_int	mem_hook = 0;
atomic_int	mem_hook_pattern = 0;
atomic_int	mem_http_slot = 0;
atomic_int	mem_log_entry = 0;
atomic_int	mem_log_what = 0;
atomic_int	mem_log_tid = 0;
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
atomic_int	mem_subject = 0;
atomic_int	mem_text = 0;
atomic_int	mem_to_s = 0;
atomic_int	mem_upgraded = 0;

#ifdef MEM_DEBUG
static void
print_stats() {
    Rec	r;

    printf("********************************************************************************\n");
    pthread_mutex_lock(&lock);

    while (NULL != (r = recs)) {
	int	cnt = 1;
	Rec	prev = NULL;
	Rec	next = NULL;
	Rec	r2;
	
	recs = r->next;
	for (r2 = recs; NULL != r2; r2 = next) {
	    next = r2->next;
	    if (r->file == r2->file && r->line == r2->line) {
		cnt++;
		if (NULL == prev) {
		    recs = r2->next;
		} else {
		    prev->next = r2->next;
		}
		free(r2);
	    } else {
		prev = r2;
	    }
	}
	printf("%16s:%3d %-12s allocated and not freed %4d times.\n", r->file, r->line, r->type + 4, cnt);
	free(r);
    }
    pthread_mutex_unlock(&lock);	

    printf("********************************************************************************\n");
#if 0
    printf("memory statistics\n");
    printf("  mem_bind:         %d\n", mem_bind);
    printf("  mem_cb:           %d\n", mem_cb);
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
    printf("  mem_log_tid:      %d\n", mem_log_tid);
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
    printf("  mem_text:         %d\n", mem_text);
    printf("  mem_to_s:         %d\n", mem_to_s);
#endif
}
#endif

#if 0
static void*
handle_gc(void *x) {
    rb_gc_enable();
    rb_gc();
    return NULL;
}
#endif

void
debug_report() {
#ifdef MEM_DEBUG
    // TBD how can ownership of GVL be determined?
    //rb_thread_call_with_gvl(handle_gc, NULL);
    print_stats();
#endif
}

void
debug_rreport() {
#ifdef MEM_DEBUG
    rb_gc_enable();
    rb_gc();
    print_stats();
#endif
}

void
debug_add(void *ptr, const char *type, const char *file, int line) {
    Rec	r = (Rec)malloc(sizeof(struct _Rec));
    
    r->ptr = ptr;
    r->type = type;
    r->file = file;
    r->line = line;
    pthread_mutex_lock(&lock);	
    r->next = recs;
    recs = r;
    pthread_mutex_unlock(&lock);	
}

void
debug_update(void *orig, void *ptr, const char *type, const char *file, int line) {
    Rec	r;
    
    pthread_mutex_lock(&lock);
    for (r = recs; NULL != r; r = r->next) {
	if (orig == r->ptr) {
	    r->ptr = ptr;
	    break;
	}
    }
    pthread_mutex_unlock(&lock);	
    if (NULL == r) {
	printf("Realloc at %s:%d (%p) not allocated.\n", file, line, orig);
    }
}

void
debug_del(void *ptr, const char *file, int line) {
    Rec	r = NULL;
    Rec	prev = NULL;
    
    pthread_mutex_lock(&lock);
    for (r = recs; NULL != r; r = r->next) {
	if (ptr == r->ptr) {
	    if (NULL == prev) {
		recs = r->next;
	    } else {
		prev->next = r->next;
	    }
	    break;
	}
	prev = r;
    }
    pthread_mutex_unlock(&lock);
    if (NULL == r) {
	printf("Free at %s:%d (%p) not allocated or already freed.\n", file, line, ptr);
    } else {
	free(r);
    }
}
