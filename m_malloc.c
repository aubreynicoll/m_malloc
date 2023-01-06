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

#include "m_malloc.h"

#include <libc.h>

#if (CHECK_HEAP)
#define fail_if(exp, ...)                             \
	{                                             \
		if (exp) {                            \
			fprintf(stderr, __VA_ARGS__); \
			fprintf(stderr, "\n");        \
			exit(1);                      \
		}                                     \
	}

#define check_heap()                                                           \
	{                                                                      \
		for (Header *curr = head.nextp; curr != &head;                 \
		     curr = curr->nextp) {                                     \
			fail_if(                                               \
			    is_alloc(curr),                                    \
			    "check_heap: found allocated block in free list"); \
		}                                                              \
	}
#else
#define fail_if(exp, ...)
#define check_heap()
#endif /* CHECK_HEAP */

#if (PRINT_DEBUG_INFO)
#define print_debug_info(...) printf(__VA_ARGS__)

#define print_free_list()                                                  \
	{                                                                  \
		print_debug_info("freelist:\n");                           \
		for (Header *curr = head.nextp; curr != &head;             \
		     curr = curr->nextp) {                                 \
			print_debug_info("%p: size %zu, alloc %d, ", curr, \
					 get_size(curr), is_alloc(curr));  \
		}                                                          \
		print_debug_info("\n");                                    \
	}

#else
#define print_debug_info(...)
#define print_free_list()
#endif /* PRINT_DEBUG_INFO */

/* most restrictive alignment */
#define ALIGNMENT _Alignof(max_align_t)
#define ALIGNMENT_MASK (ALIGNMENT - 1)

#define HEADER_SIZE offsetof(Header, nextp)
#define HEADER_OFFSET (ALIGNMENT - HEADER_SIZE)

#define GET_ALIGNED_SIZE(size) \
	((HEADER_SIZE + (size) + ALIGNMENT_MASK) & ~ALIGNMENT_MASK)

#define GET_ALIGNED_HEADER(size) (GET_ALIGNED_SIZE(size) - HEADER_SIZE)

#define ALLOC_FLAG 0x1

/**
 * * Header
 *
 * Taking inspiration from glibc, this struct is misleading in the sense that
 * part of it actually exists in the usable payload. The actual overhead is
 * sizeof(size_t). This leads to a few important facts:
 * ! 1. &nextp == the actual payload when the block is allocated
 * ! 2. &nextp must be aligned to _Alignof(max_align_t)
 * ! 3. sizeof(Header) is the smallest block size
 *
 * To keep overhead to a minimum, I've also opted (for now) to avoid the use of
 * footers for fast coalescing. Instead, I will employ a trailing pointer to
 * the previous block and check that way. This also should allow for a
 * singly-linked list, rather than a doubly-linked list, which keeps minimum
 * block size smaller as free blocks would require less space.
 *
 * While I would love to keep block sizes a multiple of the header size as seen
 * in K&R, I'd like to try to minimize overhead as much as possible. I also
 * don't want to feel like I'm just copying out of the book either. That would
 * be lame.
 */
typedef struct header Header;
struct header {
	size_t	data;
	Header *nextp; /* only used when free */
};

/* global data */
#if (CHECK_HEAP)
size_t heap_size = 0;
#endif

/* static data */
static Header	head; /* empty cyclical linked list */
static unsigned initialized;

/* function prototypes */
static size_t  get_size(Header *header);
static void    set_size(Header *header, size_t size);
static void    set_alloc(Header *header, int true);
static Header *get_nextp(Header *header);
static void    set_nextp(Header *header, Header *nextp);
static void   *get_payload_ptr(Header *header);

static void *extend_heap(size_t incr);
static void  internal_free(Header *header);

#if (CHECK_HEAP)
static int is_alloc(Header *header);
#endif

/* function definitions */
void *m_malloc(size_t size) {
	print_debug_info("called m_malloc! ");
	print_debug_info("requested size: %zu, ", size);

	if (size == 0) {
		return NULL;
	}

	if (!initialized) {
		head.nextp = &head;
		initialized = 1;
	}

	size = GET_ALIGNED_SIZE(size);
	print_debug_info("aligned size: %zu\n", size);

	for (Header *prev_block = &head, *curr_block = get_nextp(&head);;
	     prev_block = curr_block, curr_block = get_nextp(curr_block)) {
		if (curr_block == &head) {
			// get more memory
			Header *new_block = extend_heap(size + ALIGNMENT);
			if (new_block == NULL) {
				return NULL;
			}

			if ((uintptr_t)new_block % ALIGNMENT != HEADER_OFFSET) {
				// Must round up to HEADER_OFFSET, so that
				// payload falls on ALIGNMENT
				uintptr_t addr = (uintptr_t)new_block;
				addr = GET_ALIGNED_HEADER(addr);
				new_block = (Header *)addr;

				// Cannot use ALIGNMENT bytes, because rounding
				set_size(new_block, size);
			} else {
				// Include ALIGNMENT bytes in size
				set_size(new_block, size + ALIGNMENT);
			}

			internal_free(new_block);

			continue;
		}

		if (get_size(curr_block) >= size) {
			set_nextp(prev_block, get_nextp(curr_block));
			set_alloc(curr_block, 1);

			print_debug_info("allocated %p\n", curr_block);
			print_free_list();

			void *payload_ptr = get_payload_ptr(curr_block);

			return payload_ptr;
		}
	}
}

void *m_calloc(size_t nmemb, size_t size) {
	size_t total_size = nmemb * size;
	if (nmemb && total_size / nmemb != size) {
		return NULL;
	}

	void *p = m_malloc(total_size);
	if (p == NULL) {
		return NULL;
	}

	memset(p, 0, total_size);
	return p;
}

void *m_realloc(void *ptr, size_t size) {
	print_debug_info("***begin realloc***\n");

	if (ptr == NULL) {
		return m_malloc(size);
	}

	size_t prev_size = get_size(ptr - HEADER_SIZE);
	void *new = m_malloc(size);

	if (new == NULL) {
		return NULL;
	}

	memcpy(new, ptr, prev_size < size ? prev_size : size);
	m_free(ptr);

	print_debug_info("***end realloc***\n");

	return new;
}

void m_free(void *ptr) {
	print_debug_info("called m_free! freed %p\n", ptr - HEADER_SIZE);

	// TODO will need to acquire mutex here
	internal_free(ptr - HEADER_SIZE);

	print_free_list();
	check_heap();
}

static size_t get_size(Header *header) {
	return header->data & ~ALIGNMENT_MASK;
}

static void set_size(Header *header, size_t size) {
	fail_if(size & ALIGNMENT_MASK,
		"set_size: size was not multiple of alignment requirement");

	header->data = size;
}

static void set_alloc(Header *header, int true) {
	if (true) {
		// set alloc bit
		header->data |= ALLOC_FLAG;
	} else {
		// clear alloc bit
		header->data &= ~ALLOC_FLAG;
	}
}

static Header *get_nextp(Header *header) {
	fail_if(is_alloc(header),
		"get_nextp: attempted to get nextp from allocated block");

	return header->nextp;
}

static void set_nextp(Header *header, Header *nextp) {
	fail_if(is_alloc(header),
		"set_nextp: attempted to set nextp in allocated block");

	header->nextp = nextp;
}

static void *get_payload_ptr(Header *header) {
	fail_if(
	    !is_alloc(header),
	    "get_payload_ptr: attempted to get payload pointer from a free block");
	fail_if((uintptr_t)&header->nextp & ALIGNMENT_MASK,
		"get_payload_ptr: usable payload not aligned properly");

	return &header->nextp;
}

static void *extend_heap(size_t incr) {
	void *old_brk = sbrk(incr);
	if (old_brk == (void *)-1) {
		return NULL;
	}

#if (CHECK_HEAP)
	heap_size += incr;
#endif

	return old_brk;
}

static void internal_free(Header *header) {
	set_alloc(header, 0);
	set_nextp(header, get_nextp(&head));
	set_nextp(&head, header);
}

#if (CHECK_HEAP)
static int is_alloc(Header *header) {
	return header->data & ALLOC_FLAG;
}
#endif
