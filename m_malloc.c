/**
 * General purpose memory allocator
 *
 * Main principles:
 * - handles arbitrary request sequences
 * - responds immediately with no reordering or buffering of requests
 * - uses only the heap
 * - blocks are aligned properly
 * - allocated blocks are not modified
 *
 * Design considerations:
 * - explicit free list (singly-linked list)
 * - first fit
 * - last-in, first-out ordering
 * - no splitting
 * - no coalescing
 * - no min sbrk increment
 * - not thread-safe
 */

#ifndef __gnu_linux__
#error "I refuse to compile for anything other than gnu/linux"
#endif

#define _GNU_SOURCE

#include "m_malloc.h"

#include <libc.h>

/**
 * Header - contains information about allocated blocks.
 */
typedef union header Header;
union header {
	struct {
		size_t size;
	} data;
	max_align_t align; /* ensure proper alignment */
};

/* function prototypes */
static Header *internal_malloc(size_t size);
static Header *internal_calloc(size_t nmemb, size_t size);
static Header *internal_realloc(Header *ptr, size_t size);
static void    internal_free(Header *ptr);

/* function definitions */
void *m_malloc(size_t size) {
	return internal_malloc(size) + 1;
}

void *m_calloc(size_t nmemb, size_t size) {
	return internal_calloc(nmemb, size) + 1;
}

void *m_realloc(void *ptr, size_t size) {
	if (ptr == NULL) {
		return internal_malloc(size) + 1;
	}
	return internal_realloc((Header *)ptr - 1, size) + 1;
}

void m_free(void *ptr) {
	internal_free((Header *)ptr - 1);
}

static Header *internal_malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}

	size += sizeof(Header);

	void *map = mmap(NULL, size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (map == MAP_FAILED) {
		errno = ENOMEM;
		return NULL;
	}

	Header *header = map;
	header->data.size = size;
	return header;
}

static Header *internal_calloc(size_t nmemb, size_t size) {
	size_t total_size = nmemb * size;
	if (nmemb && total_size / nmemb != size) {
		errno = EOVERFLOW;
		return NULL;
	}

	return internal_malloc(total_size);
}

static Header *internal_realloc(Header *header, size_t size) {
	Header *new = internal_malloc(size);
	if (new == NULL) {
		return NULL;
	}

	size_t bytes = header->data.size < new->data.size ? header->data.size
							  : new->data.size;
	memcpy(new + 1, header + 1, bytes - sizeof(Header));

	internal_free(header);

	return new;
}

static void internal_free(Header *header) {
	if (munmap(header, header->data.size) == -1) {
		perror("munmap");
		exit(EXIT_FAILURE);
	};
}
