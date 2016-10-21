#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "../hopscotch.h"
#include "../farmhash-c/farmhash.h"

#define VAL_LEN 10
#define ARRAY_SIZE 419428
#define N_THREADS 4

#define N_SEGMENTS 4
#define N_BUCKETS_IN_SEGMENT 131072
#define HOP_RANGE 32
#define ADD_RANGE 512
#define MAX_TRIES 1

#define N_ITERATIONS 2

typedef unsigned int uint;

#include <stdlib.h>

typedef struct {
	char *key;
	char *val;
} kv_item_t;

// Allocate array, threads work on different portions of this array.
kv_item_t a[ARRAY_SIZE];

hs_table_t *table;

void rand_str(char *dest, size_t length);
void *fill_from_array(void *threadid);
void *get_from_array(void *threadid);
void *remove_from_array(void *threadid);
void spawn_threads(void *(*start_routine) (void *));
void join_threads();

pthread_t threads[N_THREADS];
pthread_attr_t attr;
void *status;
int rc;
int t;

int main(int argc, char **argv)
{
	long n_items = N_SEGMENTS*N_BUCKETS_IN_SEGMENT;
	printf("Initializing test with %lu buckets over %d segments, load factor -> %.2f\n", n_items, N_SEGMENTS, (float)ARRAY_SIZE/(float)n_items);

	table = hs_new(N_SEGMENTS, 
				   N_BUCKETS_IN_SEGMENT, 
				   HOP_RANGE, 
				   ADD_RANGE, 
				   MAX_TRIES,
				   &farmhash64,
				   &strcmp);

	uint i;

	for (i = 0; i < ARRAY_SIZE; i++) {
		a[i].key = malloc(sizeof(char)*(KEYLEN+1));
		if (a[i].key == NULL) {
			printf("Allocation failed\n");
			exit(1);
		}
		a[i].val = malloc(sizeof(char)*(VAL_LEN+1));
		if (a[i].val == NULL) {
			printf("Allocation failed\n");
			exit(1);
		}
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	uint it;

	for (it = 1; it < N_ITERATIONS+1; it++) {
		printf("\n----------------\n");
		printf("Iteration %d\n", it);
		printf("----------------\n\n");


		// TEST PUT
		spawn_threads(fill_from_array);
		join_threads();

		
		// TEST COUNT
		printf("Array initialized, bucket count: %d\n", hs_count(table));


		// TEST GET
		spawn_threads(get_from_array);
		join_threads();


		// TEMP TEST: same key, different memory addr
		char *dup = strdup(a[1].key);
		char *rv1 = hs_get(table, a[1].key);
		char *rv2 = hs_get(table, dup);
		assert(strcmp(rv1, rv2) == 0);
		free(dup);


		// TEST REMOVE
		spawn_threads(remove_from_array);
		join_threads();

		printf("Removed all items, bucket count: %d\n", hs_count(table));
		assert(hs_count(table) == 0);
	}

	for (i = 0; i < ARRAY_SIZE; i++) {
		free(a[i].key);
		free(a[i].val);
	}

	hs_destroy(table);

	return 0;
}


void spawn_threads(void *(*start_routine) (void *)) {
	for (t = 0; t < N_THREADS; t++) {
		rc = pthread_create(&threads[t], &attr, start_routine, (void *)t);
		if (rc) {
			printf("Couldn't spawn threads, error %d\n", rc);
			exit(-1);
		}
	}
}


void join_threads() {
	for (t = 0; t < N_THREADS; t++) {
		rc = pthread_join(threads[t], &status);
		if (rc) {
			printf("pthread_join: %d\n", rc);
			exit(-1);
		}
	}
}


void *get_from_array(void *threadid) {
	long tid = (long)threadid;

	char *retval;
	uint hits = 0;
	uint misses = 0;
	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		kv_item_t *item = &a[tid*(ARRAY_SIZE/N_THREADS)+i];

		retval = hs_get(table, item->key);
		if (retval != NULL && strcmp(retval, item->val) == 0) {
			hits++;
		} else {
			misses++;
		}
	}
	printf("thread %lu has %d hits, and %d misses, hit ratio: %.3f\n", tid, hits, misses, (float)hits/((float)hits+(float)misses));

	pthread_exit(NULL);
}


void *remove_from_array(void *threadid) {
	long tid = (long)threadid;

	void *data;
	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		kv_item_t *item = &a[tid*(ARRAY_SIZE/N_THREADS)+i];
		data = hs_remove(table, item->key);
	}

	pthread_exit(NULL);	
}


void *fill_from_array(void *threadid) {
	long tid = (long)threadid;
	int ret;

	uint i;
	for (i = 0; i < ARRAY_SIZE/N_THREADS; i++) {
		kv_item_t *item = &a[tid*(ARRAY_SIZE/N_THREADS)+i];

		rand_str(item->key, KEYLEN);
		rand_str(item->val, VAL_LEN);
		ret = hs_put(table, item->key, item->val);
	}

	pthread_exit(NULL);
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
