#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../hopscotch.h"
#include "../farmhash-c/farmhash.h"

#define N_SEGMENTS 4
#define N_BUCKETS_IN_SEGMENT 512
#define HOP_RANGE 32
#define ADD_RANGE 512
#define MAX_TRIES 1
#define KEY_LEN 3


#define TEST(b)									\
	do {										\
		if (b) {								\
			printf("\x1B[32mPASS\e[0m\n");		\
		} else {								\
			printf("\x1B[31mFAIL\e[0m\n");		\
			exit(-1);							\
		}										\
	} while(0);									\
		


void main(void) {
	int s;
	char *rv;
	int count;

	// --- test creation ---
	hs_table_t *table;

	printf("TESTING hs_new scary args (N_SEGMENTS) invalid: ");
	table = hs_new(7, N_BUCKETS_IN_SEGMENT, ADD_RANGE, MAX_TRIES, &farmhash64, &strcmp, KEY_LEN);
	TEST(table == NULL);

	printf("TESTING hs_new scary args (N_BUCKETS_IN_SEGMENT) invalid: ");
	table = hs_new(N_SEGMENTS, 513, ADD_RANGE, MAX_TRIES, &farmhash64, &strcmp, KEY_LEN);
	TEST(table == NULL);

	printf("TESTING hs_new correctly: ");
	table = hs_new(N_SEGMENTS, N_BUCKETS_IN_SEGMENT, ADD_RANGE, MAX_TRIES, &farmhash64, &strcmp, KEY_LEN);
	TEST(table != NULL);


	// --- test put ---
	printf("TESTING hs_put: ");
	s = hs_put(table, "foo", "bar");
	TEST(!s);

	printf("TESTING hs_put duplicate invalid: ");
	s = hs_put(table, "foo", "baz");
	TEST(s);

	printf("TESTING hs_count after put: ");
	count = hs_count(table);
	TEST(count == 1);

	// --- test get ---
	printf("TESTING hs_get: ");
	rv = hs_get(table, "foo");
	TEST(!strcmp(rv, "bar"));

	printf("TESTING hs_get nonexisting key invalid: ");
	rv = hs_get(table, "poo");
	TEST(rv == NULL);


	// --- test remove ---
	printf("TESTING hs_remove: ");
	rv = hs_remove(table, "foo");
	TEST(!strcmp(rv, "bar"));

	printf("TESTING hs_count after remove: ");
	count = hs_count(table);
	TEST(count == 0);

	printf("TESTING hs_get after hs_remove invalid: ");
	rv = hs_get(table, "foo");
	TEST(rv == NULL);

	printf("TESTING hs_remove on nonexisting key invalid: ");
	rv = hs_remove(table, "foo");
	TEST(rv == NULL);

	hs_destroy(table);

	printf("ALL PASSED!\n");
}

