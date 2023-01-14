#ifndef __m_malloc_h__
#define __m_malloc_h__

#ifndef __gnu_linux__
#error "targets GNU/Linux. please compile and run on Linux with glibc."
#endif

#ifndef __GNUC__
#error "uses GNU extensions. please compile using GCC."
#endif

#include <stddef.h>

void *m_malloc(size_t size);
void *m_calloc(size_t nmemb, size_t size);
void *m_realloc(void *ptr, size_t size);
void  m_free(void *);

#endif
