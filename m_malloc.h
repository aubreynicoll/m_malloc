/**
 * @file m_malloc.h
 * @author aubrey nicoll (aubrey.nicoll@gmail.com)
 * @brief header file for m_malloc & friends
 * @version 0.1
 * @date 2022-12-27
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stddef.h>

void *m_malloc(size_t size);
void *m_calloc(size_t nmemb, size_t size);
void *m_realloc(void *ptr, size_t size);
void  m_free(void *);
