#include <stdio.h>
#include "../hopscotch.h"
#include "../hash.h"

#include <stdlib.h>

hash_t calculate_hash(char *str)
{
	uint32_t hash = HASH_INITIAL;
	int i = 0;
	while (str[i] != 0) {
		hash = HASH_NEXT_CHAR(hash, str[i]);
		i++;
	}
	return HASH_FINAL(hash);
}

int main(int argc, char **argv)
{
	hash_function = &calculate_hash;

	printf("creating table\n");
	hs_table_t *table = hs_new(4, 512, 32, 256, 2);

	printf("putting first item\n");
	hs_put(table, "John Doe", "The sharp one.");

	printf("putting second item\n");
	hs_put(table, "Ola Normann", "The crazy one.");

	printf("buckets should be 2: %d\n", hs_sum_bucket_count(table));

	printf("getting john doe\n");
	hs_get(table, "John Doe");

	printf("removing ola normann\n");
	hs_remove(table, "Ola Normann");

	printf("buckets should be 1: %d\n", hs_sum_bucket_count(table));

	printf("destroying table\n");
	hs_destroy(table);

	return 0;
}
