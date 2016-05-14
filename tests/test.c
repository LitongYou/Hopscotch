#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "../hopscotch.h"
#include "../farmhash-c/farmhash.h"

#define VALLEN 10
#define ARRAY_SIZE 419430
#define N_THREADS 4

#define N_SEGMENTS 4
#define N_BUCKETS_IN_SEGMENT 131072
#define HOP_RANGE 32
#define ADD_RANGE 216
#define MAX_TRIES 1

typedef unsigned int uint;

#include <stdlib.h>

typedef struct {
	char *key;
	char *val;
} kv_t;

// Allocate array, threads work on different portions of this array.
kv_t a[ARRAY_SIZE];

hs_table_t *table;

void rand_str(char *dest, size_t length);
void *fill_array(void *threadid);
void *get_array(void *threadid);
void *remove_array(void *threadid);

int main(int argc, char **argv)
{
	long n_items = N_SEGMENTS*N_BUCKETS_IN_SEGMENT;
	printf("Initializing test with %lu buckets over %d segments, load factor -> %.2f\n", n_items, 
		   N_SEGMENTS, (float)ARRAY_SIZE/(float)n_items);

	hash_function = &farmhash32;

	table = hs_new(N_SEGMENTS, 
				   N_BUCKETS_IN_SEGMENT, 
				   HOP_RANGE, 
				   ADD_RANGE, 
				   MAX_TRIES);

	uint i;

	for (i = 0; i < ARRAY_SIZE; i++) {
		a[i].key = malloc(sizeof(char)*(KEYLEN+1));
		if (a[i].key == NULL) {
			printf("Allocation failed\n");
		}
		a[i].val = malloc(sizeof(char)*(VALLEN+1));
		if (a[i].val == NULL) {
			printf("Allocation failed\n");
		}
	}

	const int n_iterations = 3;
	uint it;

	for (it = 1; it < n_iterations; it++) {
		printf("----------------\n");
		printf("Iteration %d\n", it);
		printf("----------------\n\n");

		long t;
		int rc;
		pthread_t threads[N_THREADS];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		void *status;

		// Fill array
		printf("Filling array\n");
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_create(&threads[t], &attr, fill_array, (void *)t);
			if (rc) {
				printf("Couldn't spawn threads, error %d\n", rc);
				exit(-1);
			}
		}

		// Join threads
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_join(threads[t], &status);
			if (rc) {
				printf("pthread_join: %d\n", rc);
				exit(-1);
			}
		}

		printf("Array initialized, bucket count: %d\n", hs_sum_bucket_count(table));

		// Test get(key)
		printf("Getting items\n");
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_create(&threads[t], &attr, get_array, (void *)t);
			if (rc) {
				printf("Couldn't spawn threads, error %d\n", rc);
				exit(-1);
			}
		}

		// Join threads
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_join(threads[t], &status);
			if (rc) {
				printf("pthread_join: %d\n", rc);
				exit(-1);
			}
		}

		// Remove all items
		printf("Removing items\n");
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_create(&threads[t], &attr, remove_array, (void *)t);
			if (rc) {
				printf("Couldn't spawn threads, error %d\n", rc);
				exit(-1);
			}
		}

		// Join threads
		for (t = 0; t < N_THREADS; t++) {
			rc = pthread_join(threads[t], &status);
			if (rc) {
				printf("pthread_join: %d\n", rc);
				exit(-1);
			}
		}

		printf("Removed all items, bucket count: %d\n", hs_sum_bucket_count(table));
		assert(hs_sum_bucket_count(table) == 0);
	}

	pthread_exit(NULL);
	hs_destroy(table);

	return 0;
}

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

void *get_array(void *threadid) {
	long tid = (long)threadid;

	uint hits = 0;
	uint misses = 0;
	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		if (hs_get(table, a[tid*(ARRAY_SIZE/N_THREADS)+i].key) == a[tid*(ARRAY_SIZE/N_THREADS)+i].val) {
			hits++;
		} else {
			misses++;
		}
	}
	printf("thread %lu has %d hits, and %d misses, hit ratio: %.3f\n", tid, hits, misses, (float)hits/((float)hits+(float)misses));

	pthread_exit(NULL);
}

void *remove_array(void *threadid) {
	long tid = (long)threadid;

	void *data;
	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		data = hs_remove(table, a[tid*(ARRAY_SIZE/N_THREADS)+i].key);
		free(data);
	}

	pthread_exit(NULL);	
}

void *fill_array(void *threadid) {
	long tid = (long)threadid;

	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		rand_str(a[tid*(ARRAY_SIZE/N_THREADS)+i].key, KEYLEN);
		rand_str(a[tid*(ARRAY_SIZE/N_THREADS)+i].val, VALLEN);
		hs_put(table, a[tid*(ARRAY_SIZE/N_THREADS)+i].key, a[tid*(ARRAY_SIZE/N_THREADS)+i].val);
	}

	pthread_exit(NULL);
}
