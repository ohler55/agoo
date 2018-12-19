// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <ruby.h>
//#include <ruby/thread.h>

#include "debug.h"

typedef struct _rec {
    struct _rec	*next;
    const void	*ptr;
    size_t	size;
    const char	*file;
    int		line;
} *Rec;

typedef struct _rep {
    struct _rep	*next;
    size_t	size;
    const char	*file;
    int		line;
    int		cnt;
} *Rep;

#ifdef MEM_DEBUG

static pthread_mutex_t	lock = PTHREAD_MUTEX_INITIALIZER;
static Rec		recs = NULL;

void
agoo_alloc(const void *ptr, size_t size, const char *file, int line) {
    Rec		r = (Rec)malloc(sizeof(struct _rec));
    
    r->ptr = ptr;
    r->size = size;
    r->file = file;
    r->line = line;
    pthread_mutex_lock(&lock);	
    r->next = recs;
    recs = r;
    pthread_mutex_unlock(&lock);	
}

void*
agoo_malloc(size_t size, const char *file, int line) {
    void	*ptr = malloc(size);
    Rec		r = (Rec)malloc(sizeof(struct _rec));
    
    r->ptr = ptr;
    r->size = size;
    r->file = file;
    r->line = line;
    pthread_mutex_lock(&lock);	
    r->next = recs;
    recs = r;
    pthread_mutex_unlock(&lock);	

    return ptr;
}

void*
agoo_realloc(void *orig, size_t size, const char *file, int line) {
    void	*ptr = realloc(orig, size);
    Rec		r;
    
    pthread_mutex_lock(&lock);
    for (r = recs; NULL != r; r = r->next) {
	if (orig == r->ptr) {
	    r->ptr = ptr;
	    r->size = size;
	    r->file = file;
	    r->line = line;
	    break;
	}
    }
    pthread_mutex_unlock(&lock);	
    if (NULL == r) {
	printf("Realloc at %s:%d (%p) not allocated.\n", file, line, orig);
    }
    return ptr;
}

void
agoo_freed(void *ptr, const char *file, int line) {
    if (NULL != ptr) {
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
}

void
agoo_free(void *ptr, const char *file, int line) {
    if (NULL != ptr) {
	agoo_freed(ptr, file, line);
	free(ptr);
    }
}

#endif

#ifdef MEM_DEBUG

static Rep
update_reps(Rep reps, Rec r) {
    Rep	rp = reps;

    for (; NULL != rp; rp = rp->next) {
	if (rp->line == r->line && (rp->file == r->file || 0 == strcmp(rp->file, r->file))) {
	    rp->size += r->size;
	    rp->cnt++;
	    break;
	}
    }
    if (NULL == rp) {
	rp = (Rep)malloc(sizeof(struct _rep));
	rp->size = r->size;
	rp->file = r->file;
	rp->line = r->line;
	rp->cnt = 1;
	rp->next = reps;
	reps = rp;
    }
    return reps;
}

static void
print_stats() {
    printf("\n*** Memory Usage Report ********************************************************\n");
    pthread_mutex_lock(&lock);

    if (NULL == recs) {
	printf("No memory leaks\n");
    } else {
	Rep	reps = NULL;
	Rep	rp;
	Rec	r;
	size_t	leaked = 0;

	for (r = recs; NULL != r; r = r->next) {
	    reps = update_reps(reps, r);
	}
	while (NULL != (rp = reps)) {
	    reps = rp->next;
	    printf("%16s:%3d %8lu bytes over %d occurances allocated and not freed.\n", rp->file, rp->line, rp->size, rp->cnt);
	    leaked += rp->size;
	    free(rp);
	}
	printf("%lu bytes leaked\n", leaked);
    }
    pthread_mutex_unlock(&lock);	
    printf("********************************************************************************\n");
}
#endif

void
debug_report() {
#ifdef MEM_DEBUG
    print_stats();
#endif
}
