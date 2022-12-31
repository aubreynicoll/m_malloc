#ifndef __m_malloc_h__
#define __m_malloc_h__

#include <stddef.h>

void *m_malloc(size_t size);
void *m_calloc(size_t nmemb, size_t size);
void *m_realloc(void *ptr, size_t size);
void  m_free(void *);

#endif
