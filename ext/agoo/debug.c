// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static const char	mem_pad[] = "--- This is a memory pad and should not change until being freed. ---";

void
agoo_alloc(const void *ptr, size_t size, const char *file, int line) {
    Rec		r = (Rec)malloc(sizeof(struct _rec));

    if (NULL != r) {
	r->ptr = ptr;
	r->size = size;
	r->file = file;
	r->line = line;
	pthread_mutex_lock(&lock);
	r->next = recs;
	recs = r;
	pthread_mutex_unlock(&lock);
    }
}

void*
agoo_malloc(size_t size, const char *file, int line) {
    void	*ptr = malloc(size + sizeof(mem_pad));

    if (NULL != ptr) {
	Rec	r = (Rec)malloc(sizeof(struct _rec));

	if (NULL != r) {
	    strcpy(((char*)ptr) + size, mem_pad);
	    r->ptr = ptr;
	    r->size = size;
	    r->file = file;
	    r->line = line;
	    pthread_mutex_lock(&lock);
	    r->next = recs;
	    recs = r;
	    pthread_mutex_unlock(&lock);
	} else {
	    free(ptr);
	    ptr = NULL;
	}
    }
    return ptr;
}

void*
agoo_calloc(size_t count, size_t size, const char *file, int line) {
    void	*ptr;

    size *= count;
    if (NULL != (ptr = malloc(size + sizeof(mem_pad)))) {
	Rec	r = (Rec)malloc(sizeof(struct _rec));

	if (NULL != r) {
	    memset(ptr, 0, size);
	    strcpy(((char*)ptr) + size, mem_pad);
	    r->ptr = ptr;
	    r->size = size;
	    r->file = file;
	    r->line = line;
	    pthread_mutex_lock(&lock);
	    r->next = recs;
	    recs = r;
	    pthread_mutex_unlock(&lock);
	} else {
	    free(ptr);
	    ptr = NULL;
	}
    }
    return ptr;
}

void*
agoo_realloc(void *orig, size_t size, const char *file, int line) {
    void	*ptr = realloc(orig, size + sizeof(mem_pad));
    Rec		r;

    if (NULL != ptr) {
	strcpy(((char*)ptr) + size, mem_pad);
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
    }
    return ptr;
}

char*
agoo_strdup(const char *str, const char *file, int line) {
    size_t	size = strlen(str) + 1;
    char	*ptr = (char*)malloc(size + sizeof(mem_pad));

    if (NULL != ptr) {
	Rec	r = (Rec)malloc(sizeof(struct _rec));

	if (NULL != r) {
	    strcpy(ptr, str);
	    strcpy(((char*)ptr) + size, mem_pad);
	    r->ptr = (void*)ptr;
	    r->size = size;
	    r->file = file;
	    r->line = line;
	    pthread_mutex_lock(&lock);
	    r->next = recs;
	    recs = r;
	    pthread_mutex_unlock(&lock);
	} else {
	    free(ptr);
	    ptr = NULL;
	}
    }
    return ptr;
}

char*
agoo_strndup(const char *str, size_t len, const char *file, int line) {
    size_t	size = len + 1;
    char	*ptr = (char*)malloc(size + sizeof(mem_pad));

    if (NULL != ptr) {
	Rec	r = (Rec)malloc(sizeof(struct _rec));

	if (NULL != r) {
	    memcpy(ptr, str, len);
	    ptr[len] = '\0';
	    strcpy(((char*)ptr) + size, mem_pad);
	    r->ptr = (void*)ptr;
	    r->size = size;
	    r->file = file;
	    r->line = line;
	    pthread_mutex_lock(&lock);
	    r->next = recs;
	    recs = r;
	    pthread_mutex_unlock(&lock);
	} else {
	    free(ptr);
	    ptr = NULL;
	}
    }
    return ptr;
}

void
agoo_mem_check(void *ptr, const char *file, int line) {
    if (NULL != ptr) {
	Rec	r = NULL;

	pthread_mutex_lock(&lock);
	for (r = recs; NULL != r; r = r->next) {
	    if (ptr == r->ptr) {
		break;
	    }
	}
	pthread_mutex_unlock(&lock);
	if (NULL == r) {
	    printf("Memory check at %s:%d (%p) not allocated or already freed.\n", file, line, ptr);
	} else {
	    char	*pad = (char*)r->ptr + r->size;

	    if (0 != strcmp(mem_pad, pad)) {
		uint8_t	*p;
		uint8_t	*end = (uint8_t*)pad + sizeof(mem_pad);

		printf("Check - Memory at %s:%d (%p) write outside allocated.\n", file, line, ptr);
		for (p = (uint8_t*)pad; p < end; p++) {
		    if (0x20 < *p && *p < 0x7f) {
			printf("%c  ", *p);
		    } else {
			printf("%02x ", *(uint8_t*)p);
		    }
		}
		printf("\n");
	    }
	}
    }
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
	    char	*pad = (char*)r->ptr + r->size;

	    if (0 != strcmp(mem_pad, pad)) {
		uint8_t	*p;
		uint8_t	*end = (uint8_t*)pad + sizeof(mem_pad);

		printf("Memory at %s:%d (%p) write outside allocated.\n", file, line, ptr);
		for (p = (uint8_t*)pad; p < end; p++) {
		    if (0x20 < *p && *p < 0x7f) {
			printf("%c  ", *p);
		    } else {
			printf("%02x ", *(uint8_t*)p);
		    }
		}
		printf("\n");
	    }
	    free(r);
	}
    }
}

void
agoo_free(void *ptr, const char *file, int line) {
    agoo_freed(ptr, file, line);
    free(ptr);
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
    if (NULL == rp &&
	NULL != (rp = (Rep)malloc(sizeof(struct _rep)))) {
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
    printf("\n--- Memory Usage Report --------------------------------------------------------\n");
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
    printf("--------------------------------------------------------------------------------\n");
}
#endif

void
debug_report() {
#ifdef MEM_DEBUG
    print_stats();
#endif
}
