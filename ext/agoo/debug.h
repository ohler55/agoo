// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_DEBUG_H
#define AGOO_DEBUG_H

#include <stdlib.h>

#ifdef MEM_DEBUG

#define AGOO_MALLOC(size) agoo_malloc(size, __FILE__, __LINE__)
#define AGOO_ALLOC(ptr, size) agoo_alloc(ptr, size, __FILE__, __LINE__)
#define AGOO_REALLOC(ptr, size) agoo_realloc(ptr, size, __FILE__, __LINE__)
#define AGOO_FREE(ptr) agoo_free(ptr, __FILE__, __LINE__)
#define AGOO_FREED(ptr) agoo_freed(ptr, __FILE__, __LINE__)

extern void*	agoo_malloc(size_t size, const char *file, int line);
extern void*	agoo_realloc(void *ptr, size_t size, const char *file, int line);
extern void	agoo_free(void *ptr, const char *file, int line);
extern void	agoo_freed(void *ptr, const char *file, int line);
extern void	agoo_alloc(const void *ptr, size_t size, const char *file, int line);

#else

#define AGOO_MALLOC(size) malloc(size)
#define AGOO_ALLOC(ptr, size) {}
#define AGOO_REALLOC(ptr, size) realloc(ptr, size)
#define AGOO_FREE(ptr) free(ptr)
#define AGOO_FREED(ptr) {}

#endif

extern void	debug_report();

#endif /* AGOO_DEBUG_H */
