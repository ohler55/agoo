// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_DEBUG_H
#define AGOO_DEBUG_H

#include <stdlib.h>
#include <string.h>

#ifdef MEM_DEBUG

#define AGOO_MALLOC(size) agoo_malloc(size, __FILE__, __LINE__)
#define AGOO_CALLOC(count, size) agoo_calloc(count, size, __FILE__, __LINE__)
#define AGOO_ALLOC(ptr, size) agoo_alloc(ptr, size, __FILE__, __LINE__)
#define AGOO_REALLOC(ptr, size) agoo_realloc(ptr, size, __FILE__, __LINE__)
#define AGOO_STRDUP(str) agoo_strdup(str, __FILE__, __LINE__)
#define AGOO_STRNDUP(str, len) agoo_strndup(str, len, __FILE__, __LINE__)

#define AGOO_FREE(ptr) agoo_free(ptr, __FILE__, __LINE__)
#define AGOO_FREED(ptr) agoo_freed(ptr, __FILE__, __LINE__)
#define AGOO_MEM_CHECK(ptr) agoo_mem_check(ptr, __FILE__, __LINE__)

extern void*	agoo_malloc(size_t size, const char *file, int line);
extern void*	agoo_calloc(size_t count, size_t size, const char *file, int line);
extern void*	agoo_realloc(void *ptr, size_t size, const char *file, int line);
extern char*	agoo_strdup(const char *str, const char *file, int line);
extern char*	agoo_strndup(const char *str, size_t len, const char *file, int line);
extern void	agoo_free(void *ptr, const char *file, int line);
extern void	agoo_freed(void *ptr, const char *file, int line);
extern void	agoo_alloc(const void *ptr, size_t size, const char *file, int line);
extern void	agoo_mem_check(void *ptr, const char *file, int line);

#else

#define AGOO_MALLOC(size) malloc(size)
#define AGOO_CALLOC(count, size) calloc(count, size)
#define AGOO_ALLOC(ptr, size) {}
#define AGOO_REALLOC(ptr, size) realloc(ptr, size)
#define AGOO_STRDUP(str) strdup(str)
#define AGOO_STRNDUP(str, len) strndup(str, len)
#define AGOO_FREE(ptr) free(ptr)
#define AGOO_FREED(ptr) {}
#define AGOO_MEM_CHECK(ptr) {}

#endif

extern void	debug_report();

#endif /* AGOO_DEBUG_H */
