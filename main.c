/**
 * Driver program for testing m_malloc.h
 *
 * basically makes a bunch of random calls to m_malloc, m_realloc, and m_free,
 * while the sanity checks in m_malloc detect error conditions
 */

#include <libc.h>

#include "m_malloc.h"

#define BUFSIZE 10
#define MAX_REQUEST_SIZE 8
#define MAX_REQUESTS 10000
#define REALLOC_CHANCE 50

/**
 * A driver job. Represents a driver-allocated block of memory, its size, and a
 * hash of its bytes.
 */
typedef struct job Job;
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
	while (p != end) {
		*p++ = m_rand(256);
	}
}

/**
 * Generate a hash from an array of bytes.
 */
unsigned long hash(unsigned char *p, size_t size) {
	unsigned long  h = 1;
	unsigned char *end = p + size;

	while (p != end) {
		h = h * 6364136223846793005UL + *p++;
	}

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
	printf("{.p = %p, .size = %zu, .hash = %lx}\n", job->p, job->size,
	       job->hash);
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

	unsigned malloc_count = 0;
	unsigned free_count = 0;

#if (CHECK_HEAP)
	size_t max_payload = 0;
	size_t curr_payload = 0;
#endif

	clock_t clocks = 0;
	double	execution_time;

	for (int i = 0; i < MAX_REQUESTS; i++) {
		size_t j = m_rand(BUFSIZE);

		if (jobs[j].p == NULL) {
			// malloc
			size_t requested_size =
			    m_rand(MAX_REQUEST_SIZE - 1) + 1;

			clock_t start = clock();
			void   *p = m_malloc(requested_size);
			clocks += clock() - start;

#if (CHECK_HEAP)
			curr_payload += requested_size;
			max_payload = curr_payload > max_payload ? curr_payload
								 : max_payload;
#endif

			if (p == NULL) {
				printf("m_malloc returned null\n");
				exit(EXIT_FAILURE);
			}

			initialize_job(&jobs[j], p, requested_size);
			printf("allocated: ");
			print_job(&jobs[j]);

			++malloc_count;
		} else {
			// free or realloc
			printf("hash check: ");
			print_job(&jobs[j]);
			if (!check_hash(&jobs[j])) {
				printf("hash check failed");
				exit(EXIT_FAILURE);
			};
			printf("hash check successful!\n");

			if (m_rand(100) < REALLOC_CHANCE) {
				// realloc
				size_t requested_size =
				    m_rand(MAX_REQUEST_SIZE - 1) + 1;

				clock_t start = clock();
				void *p = m_realloc(jobs[j].p, requested_size);
				clocks += clock() - start;

#if (CHECK_HEAP)
				curr_payload -= jobs[j].size;
				curr_payload += requested_size;
				max_payload = curr_payload > max_payload
						  ? curr_payload
						  : max_payload;
#endif

				if (p == NULL) {
					printf("m_realloc returned null\n");
					exit(EXIT_FAILURE);
				}

				initialize_job(&jobs[j], p, requested_size);
				printf("reallocated: ");
				print_job(&jobs[j]);

				++malloc_count;
				++free_count;
			} else {
				// free
				clock_t start = clock();
				m_free(jobs[j].p);
				clocks += clock() - start;

#if (CHECK_HEAP)
				curr_payload -= jobs[j].size;
#endif

				printf("freed: ");
				print_job(&jobs[j]);
				clear_job(&jobs[j]);

				++free_count;
			}
		}
	}

	execution_time = (double)clocks / CLOCKS_PER_SEC;

	printf(
	    "calls to malloc: %d\ncalls to free: %d\nexecution time (seconds): %f\n",
	    malloc_count, free_count, execution_time);
	printf("secs/call: %f, calls/sec: %f\n",
	       execution_time / (malloc_count + free_count),
	       (malloc_count + free_count) / execution_time);
#if (CHECK_HEAP)
	printf("total heap size: %zu\n", heap_size);
	printf("peak utilization: %f%%\n",
	       (double)max_payload / heap_size * 100);
#endif
}
