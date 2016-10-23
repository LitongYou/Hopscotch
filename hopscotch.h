#ifndef __HOPSCOTCH_H_
#define __HOPSCOTCH_H_

#include <stdint.h>
#include <stddef.h>

#define MUTEX

// Change these to your needs:
// type of bitmap_t dictates hop_range, i.e. the range from base bucket that buckets
// must fall within, bigger bitmap yields more options, however the buckets becomes
// bigger which may have some performance implications.
typedef uint32_t bitmap_t;

typedef struct hs_table_s hs_table_t;

// - n_segments dictates the concurrency level, reads are concurrent and linearizable
//   however any update to the table (i.e, add or remove) locks on segment granularity.
// - n_buckets_in_segment : power of two number,  this is size of segment, each time
//   table is resized, number of segments doubles up, to a maximum of max_segments.
// - add_range is the number of buckets searched in order to find a free bucket for
//   displacement closer to base bucket.  When no free buckets within  add_range of
//   initial bucket, table is resized.
// - hop_range specifies the range of which buckets that isn't base bucket must fall
//   within, if range is 64, bitmap_t must be able to hold 64 bits, for range of 32,
//   uint32_t is sufficient. the size of hop_range may have performance implications.
// - max_tries specifies how many times hs_get  will try to search through its range
//   for a consistent state.
hs_table_t *hs_new(unsigned int n_segments,
				   unsigned int n_buckets_in_segment,
				   unsigned int add_range,
				   unsigned int max_tries,
				   unsigned int (*hash_fn)(void *, size_t),
				   int (*cmp_fn)(void *k1, void *k2),
				   size_t key_len);

int hs_put(hs_table_t *table, void *key, void *data);

void *hs_get(hs_table_t *table, void *key);

void *hs_remove(hs_table_t *table, void *key);

void hs_destroy(hs_table_t *table);

unsigned int hs_count(hs_table_t *table);

#endif

// EOF
