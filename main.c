/**
 * Driver program for testing m_malloc.h
 *
 * basically makes a bunch of random calls to m_malloc, m_realloc, and m_free,
 * while the sanity checks in m_malloc detect error conditions
 */

#include <libc.h>

#include "m_malloc.h"

#define BUFSIZE 10
#define MAX_REQUEST_SIZE 64000
#define MAX_REQUESTS 10000
#define REALLOC_CHANCE 10

/**
 * Driver options
 */
typedef struct options Options;
struct options {
	int test_libc_malloc;
	int verbose;
};

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
 * Function pointer type for malloc
 */
typedef void *(*malloc_t)(size_t);

/**
 * Function pointer type for calloc
 */
typedef void *(*calloc_t)(size_t, size_t);

/**
 * Function pointer type for realloc
 */
typedef void *(*realloc_t)(void *, size_t);

/**
 * Function pointer type for free
 */
typedef void (*free_t)(void *);

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

/**
 * Get current position of brk
 */
uintptr_t getbrk(void) {
	void *brk = sbrk(0);
	if (brk == (void *)-1) {
		perror("sbrk");
		exit(EXIT_FAILURE);
	}

	return (uintptr_t)brk;
}

/**
 * initialize cli args to defaults
 */
Options *initialize_options(Options *options) {
	*options = (Options){.test_libc_malloc = 0, .verbose = 0};
	return options;
}

/**
 * Parse cli args
 */
void parse_options(Options *options, int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "gv")) != -1) {
		switch (opt) {
			case 'g':
				options->test_libc_malloc = 1;
				break;
			case 'v':
				options->verbose = 1;
				break;
			default:
				fprintf(stderr, "accepted flags: -g -v");
				exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL); /* prevent printf from calling malloc */
	srand(time(NULL));

	/* check flags */
	Options config;
	parse_options(initialize_options(&config), argc, argv);

	/* assign function pointers */
	malloc_t  mallocp;
	realloc_t reallocp;
	free_t	  freep;
	if (config.test_libc_malloc) {
		mallocp = malloc;
		reallocp = realloc;
		freep = free;
	} else {
		mallocp = m_malloc;
		reallocp = m_realloc;
		freep = m_free;
	}

	Job jobs[BUFSIZE] = {NULL};

	unsigned malloc_count = 0;
	unsigned free_count = 0;

	uintptr_t heap_start = getbrk();
	size_t	  max_payload = 0;
	size_t	  curr_payload = 0;

	clock_t clocks = 0;
	double	execution_time;

	for (int i = 0; i < MAX_REQUESTS; i++) {
		size_t j = m_rand(BUFSIZE);

		if (jobs[j].p == NULL) {
			// malloc
			size_t requested_size =
			    m_rand(MAX_REQUEST_SIZE - 1) + 1;

			clock_t start = clock();
			void   *p = mallocp(requested_size);
			clocks += clock() - start;

			curr_payload += requested_size;
			max_payload = curr_payload > max_payload ? curr_payload
								 : max_payload;
			if (p == NULL) {
				printf("malloc returned null\n");
				exit(EXIT_FAILURE);
			}

			initialize_job(&jobs[j], p, requested_size);

			if (config.verbose) {
				printf("allocated: ");
				print_job(&jobs[j]);
			}

			++malloc_count;
		} else {
			// free or realloc
			if (config.verbose) {
				printf("hash check: ");
				print_job(&jobs[j]);
			}
			if (!check_hash(&jobs[j])) {
				printf("hash check failed");
				exit(EXIT_FAILURE);
			};
			if (config.verbose) {
				printf("hash check successful!\n");
			}

			if (m_rand(100) < REALLOC_CHANCE) {
				// realloc
				size_t requested_size =
				    m_rand(MAX_REQUEST_SIZE - 1) + 1;

				clock_t start = clock();
				void   *p = reallocp(jobs[j].p, requested_size);
				clocks += clock() - start;

				curr_payload -= jobs[j].size;
				curr_payload += requested_size;
				max_payload = curr_payload > max_payload
						  ? curr_payload
						  : max_payload;

				if (p == NULL) {
					printf("realloc returned null\n");
					exit(EXIT_FAILURE);
				}

				initialize_job(&jobs[j], p, requested_size);

				if (config.verbose) {
					printf("reallocated: ");
					print_job(&jobs[j]);
				}

				++malloc_count;
				++free_count;
			} else {
				// free
				clock_t start = clock();
				freep(jobs[j].p);
				clocks += clock() - start;

				curr_payload -= jobs[j].size;

				if (config.verbose) {
					printf("freed: ");
					print_job(&jobs[j]);
				}

				clear_job(&jobs[j]);

				++free_count;
			}
		}
	}

	/* print statistics */
	execution_time = (double)clocks / CLOCKS_PER_SEC;
	size_t heap_size = getbrk() - heap_start;

	printf(
	    "calls to malloc: %d\ncalls to free: %d\nexecution time (seconds): %f\n",
	    malloc_count, free_count, execution_time);
	printf("secs/call: %f, calls/sec: %f\n",
	       execution_time / (malloc_count + free_count),
	       (malloc_count + free_count) / execution_time);
	printf("total heap size: %zu\n", heap_size);
	printf("peak utilization: %f%%\n",
	       !heap_size ? 100 : (double)max_payload / heap_size * 100);
}
