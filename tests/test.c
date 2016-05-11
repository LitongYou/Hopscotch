#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "../hopscotch.h"
#include "../hash.h"

#define NUM_THREADS 5

#include <stdlib.h>

hash_t calculate_hash(char *str);
void rand_str(char *dest, size_t length);

int main(int argc, char **argv)
{
	hash_function = &calculate_hash;

	hs_table_t *table = hs_new(4, 512, 32, 256, 2);

	// -----------
	// SERIAL
	// -----------

	// testing put
	hs_put(table, "John Doe", "The sharp one.");
	hs_put(table, "Ola Normann", "The crazy one.");

	// testing num_threads
	printf("buckets should be 2: %d\n\n", hs_sum_bucket_count(table));

	// testing get
	char *str = malloc(40);
	str = hs_get(table, "John Doe");
	printf("Got John doe: %s\n\n", str);

	// testing remove
	str = hs_remove(table, "Ola Normann");
	printf("Removed %s\n\n", str);

	// getting removed item
	str = hs_get(table, "Ola Normann");
	printf("Getting Ola Normann after remove: %s\n", str);

	// -----------
	// PARALLEL
	// -----------

/*
	pthread_t threads[NUM_THREADS];

	// 100 random strings of len 10
	uint nputs = 100;
	char *a[100];
	uint i;
	for (i = 0; i < nputs; i++) {
		a[i] = malloc(10);
		rand_str(a[i], 10);
	}

	for (i = 0; i < nputs; i++) {
		
	}

*/
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

hash_t calculate_hash(char *str)
{
	return hashlittle(str, strlen(str), 0);
}
