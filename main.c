/**
 * Driver program for testing m_malloc.h
 *
 * basically makes a bunch of random calls to m_malloc, m_realloc, and m_free
 */

#include <libc.h>

#include "m_malloc.h"

#define BUFSIZE 1
#define MAX_REQUEST_SIZE 4096
#define MAX_REQUESTS 25

/**
 * Shorthand for struct job
 */
typedef struct job Job;

/**
 * A driver job. Represents a driver-allocated block of memory, its size, and a
 * hash of its bytes.
 */
struct job {
	void	     *p;
	size_t	      size;
	unsigned long hash;
};

/**
 * Get a random number.
 *
 * \return an unsigned integer up to, but not including, limit
 */
unsigned m_rand(unsigned limit) {
	return limit * (rand() / (RAND_MAX + 1.0));
}

/**
 * Fill an array with random bytes.
 */
void fill(unsigned char *p, size_t size) {
	unsigned char *end = p + size;
	while (p != end) *p++ = m_rand(256);
}

/**
 * Generate a hash from an array of bytes.
 */
unsigned long hash(unsigned char *p, size_t size) {
	unsigned long  h = 1;
	unsigned char *end = p + size;

	while (p != end) h = h * 6364136223846793005UL + *p++;

	return h;
}

/**
 * Initialize a Job. It is important to use this function, as it fills the
 * allocated memory and then computes a hash that is later used as a checksum.
 */
void initialize_job(Job *job, void *p, size_t size) {
	fill(p, size);
	*job = (Job){.p = p, .size = size, .hash = hash(p, size)};
}

/**
 * Clear a Job of its contents. Does not free the allocated memory.
 */
void clear_job(Job *job) {
	*job = (Job){.p = NULL, .size = 0, .hash = 0};
}

/**
 * Print a Job.
 */
void print_job(Job *job) {
	printf(
	    "{.p = %p, .size = %zu, .hash = %lx}\n", job->p, job->size,
	    job->hash
	);
}

/**
 * Check the integrity of a Job's data. Returns 1 if the data is good, else 0.
 */
int check_hash(Job *job) {
	return job->hash == hash(job->p, job->size);
}

int main(void) {
	setbuf(stdout, NULL); /* prevent printf from calling malloc */
	srand(time(NULL));

	Job jobs[BUFSIZE] = {NULL};

	for (int i = 0; i < MAX_REQUESTS; i++) {
		size_t j = m_rand(BUFSIZE);

		if (jobs[j].p == NULL) {
			// allocate
			size_t requested_size =
			    m_rand(MAX_REQUEST_SIZE - 1) + 1;
			void *p = m_calloc(1, requested_size);
			if (p == NULL) {
				printf("m_calloc returned null\n");
				exit(1);
			}
			initialize_job(&jobs[j], p, requested_size);
			printf("allocated: ");
			print_job(&jobs[j]);
		} else {
			// free or realloc
			printf("hash check: ");
			print_job(&jobs[j]);
			if (!check_hash(&jobs[j])) {
				printf("hash check failed");
				exit(1);
			};
			printf("hash check successful!\n");

			if (m_rand(100) < 50) {
				m_free(jobs[j].p);

				printf("freed: ");
				print_job(&jobs[j]);
				clear_job(&jobs[j]);

			} else {
				size_t requested_size =
				    m_rand(MAX_REQUEST_SIZE - 1) + 1;
				void *p = m_realloc(jobs[j].p, requested_size);
				if (p == NULL) {
					printf("m_realloc returned null\n");
					exit(1);
				}
				initialize_job(&jobs[j], p, requested_size);

				printf("reallocated: ");
				print_job(&jobs[j]);
			}
		}
	}
}
