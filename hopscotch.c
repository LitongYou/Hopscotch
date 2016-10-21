/*
  Copyright (c) <2016>, <Tommy Øines>
  All rights reserved.

  Redistribution  and  use  in  source  and  binary  forms,  with  or  without 
  modification, are permitted provided that the following  conditions are met:

  1. Redistributions  of  source code  must retain the above  copyright notice, 
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above  copyright notice, 
  this list of conditions and the following disclaimer in the  documentation 
  and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors 
  may be used to  endorse  or  promote  products  derived from this software 
  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
  AND  ANY  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED.  IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
  LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR 
  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT  LIMITED  TO,  PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED  AND ON ANY  THEORY  OF LIABILITY,  WHETHER IN 
  CONTRACT,  STRICT  LIABILITY,  OR TORT  (INCLUDING  NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE 
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <string.h>
#include "hopscotch.h"
#include "sync.h"

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#define MOD(n,m) (n & (m - 1))

typedef struct {
	void *data;
	void *key;
	bitmap_t hop_info;
} hs_bucket_t;

typedef struct {
	hs_bucket_t *bucket_array;
	hs_bucket_t *last_bucket;
	lock_t *lock;
	volatile unsigned int timestamp;
	unsigned int bucket_count;
} hs_segment_t;

struct hs_table_s {
	hs_segment_t *segment_array;
	unsigned int n_buckets_in_segment;
	unsigned int n_segments;
	unsigned int hop_range;
	unsigned int add_range;
	unsigned int max_tries;
	unsigned int (*hash_fn)(void *, size_t);
	int (*cmp_fn)(void *k1, void *k2);
};


// --------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
// --------------------------------------------

static unsigned int get_segment_idx(hs_table_t *table, unsigned int hash);
static void resize(hs_table_t *table);
static void find_closer_free_bucket (hs_table_t *table, 
									 hs_segment_t *seg, 
									 hs_bucket_t **free_bucket, 
									 unsigned int *dist_travelled);
static inline hs_bucket_t *find_key(hs_table_t *table, 
									hs_segment_t *seg, 
									hs_bucket_t *base_bucket, 
									void *key);

// --------------------------------------------
// PUBLIC FUNCTION DEFENITIONS
// --------------------------------------------

hs_table_t *hs_new(unsigned int n_segments,
				   unsigned int n_buckets_in_segment,
				   unsigned int hop_range, 
				   unsigned int add_range,
				   unsigned int max_tries,
				   unsigned int (*hash_fn)(void *, size_t),
				   int (*cmp_fn)(void *k1, void *k2))
{
	hs_table_t *table = (hs_table_t *)malloc(sizeof(hs_table_t));
	if (table == NULL) { goto table_alloc_failed; }

	table->n_segments = n_segments;
	table->n_buckets_in_segment = n_buckets_in_segment;
	table->hop_range = hop_range;
	table->add_range = add_range;
	table->max_tries = max_tries;
	table->hash_fn = hash_fn;
	table->cmp_fn = cmp_fn;

	table->segment_array = (hs_segment_t *)malloc(n_segments*sizeof(hs_segment_t));
	if (table->segment_array == NULL) { goto segment_alloc_failed; }

	hs_bucket_t *bucket_array = (hs_bucket_t *)malloc(n_buckets_in_segment*n_segments*sizeof(hs_bucket_t));
	if (bucket_array == NULL) { goto buckets_alloc_failed; }
	

	unsigned int i;
	for (i = 0; i < n_segments; i++) {
		hs_segment_t *seg = &(table->segment_array[i]);
		seg->lock = (lock_t *)malloc(sizeof(lock_t));
		if (seg->lock == NULL) { goto lock_alloc_failed; }
		LOCK_INIT(seg->lock);
		seg->timestamp = 0;
		seg->bucket_array = &(bucket_array[i*n_buckets_in_segment]);
		seg->last_bucket = &(seg->bucket_array[n_buckets_in_segment-1]);
		seg->bucket_count = 0;
		unsigned int j;
		for (j = 0; j < n_buckets_in_segment; j++) {
			seg->bucket_array[j].key = NULL;
			seg->bucket_array[j].hop_info = 0;
		}
	}

	return table;

 lock_alloc_failed:
	while(--i) {
		hs_segment_t *seg = &(table->segment_array[i]);
		free(seg->lock);
	}

 buckets_alloc_failed:
	free(table->segment_array);
	
 segment_alloc_failed:
	free(table);

 table_alloc_failed:

	return NULL;
}

int hs_put(hs_table_t *table, void *key, void *data)
{
	unsigned int hash = table->hash_fn(key, KEYLEN);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hash)]);
	unsigned int bucket_idx = MOD(hash, table->n_buckets_in_segment);
	hs_bucket_t *last_bucket = seg->last_bucket;

	hs_bucket_t *base_bucket = &(seg->bucket_array[bucket_idx]);
	LOCK_ACQUIRE(seg->lock);

	// bail out if entry already exists
    hs_bucket_t *bucket_exist = find_key(table, seg, base_bucket, key);
	if (bucket_exist != NULL) {
		LOCK_RELEASE(seg->lock);
		return -1;
	}

	hs_bucket_t *free_bucket = base_bucket;
	unsigned int dist_travelled;

	// find an empty bucket within ADD_RANGE
	for (dist_travelled = 0; dist_travelled < table->add_range; dist_travelled++) {
		if (free_bucket->key == NULL) {
			break;
		}
		free_bucket++;
		if (free_bucket > last_bucket) {
			free_bucket -= table->n_buckets_in_segment;
		}
	}

	if (dist_travelled < table->add_range) /* empty bucket found */ {
		do {
			if (dist_travelled < table->hop_range) {
				free_bucket->data = data;
				seg->bucket_count++;
				base_bucket->hop_info |= (1 << dist_travelled);
				free_bucket->key = key;
				LOCK_RELEASE(seg->lock);
				return 0;
			}
			find_closer_free_bucket(table, seg, &free_bucket, &dist_travelled);
			// if no bucket found within hop_range, free_bucket is set to NULL and
			// table will be resized.
		} while (free_bucket != NULL);
	}
	LOCK_RELEASE(seg->lock);

	printf("attempt to resize (not implemented)\n");
	//	resize(table);
	//	hs_put(table, key, data);
	return -1;
}

void *hs_get(hs_table_t *table, void *key)
{
	unsigned int hash = table->hash_fn(key, KEYLEN);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hash)]);
	unsigned int bucket_idx = MOD(hash, table->n_buckets_in_segment);

	hs_bucket_t *base_bucket = &(seg->bucket_array[bucket_idx]);
	hs_bucket_t *target;

	unsigned int try_counter = 0;
	unsigned int timestamp;

	// Makes <max_tries> attempts if it observes inconsistent state (this can only
	// happen if a hash is being displaced in same segment).
	do {
		timestamp = seg->timestamp;
		target = find_key(table, seg, base_bucket, key);
		if (target != NULL) {
			return target->data;
		}
		try_counter++;
	} while (try_counter < table->max_tries && timestamp != seg->timestamp);

	// Consider adding "slow path": Search all [base, base+hop_range] for key.
	// Note: slow path would have to consider wrapping of segment.

	return NULL;
}

void *hs_remove(hs_table_t *table, void *key)
{
	unsigned int hash = table->hash_fn(key, KEYLEN);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hash)]);
	unsigned int bucket_idx = MOD(hash, table->n_buckets_in_segment);

	hs_bucket_t *base_bucket = &(seg->bucket_array[bucket_idx]);
	hs_bucket_t *target;
	void *data;

	LOCK_ACQUIRE(seg->lock);	
	target = find_key(table, seg, base_bucket, key);
	if (target != NULL) {
		data = target->data;
		target->data = NULL;
		seg->bucket_count--;
		base_bucket->hop_info &= ~(1 << (target - base_bucket));
		target->key = NULL;
		LOCK_RELEASE(seg->lock);
		return data;
	}
	LOCK_RELEASE(seg->lock);

	return NULL;
}

void hs_destroy(hs_table_t *table)
{
	unsigned int i;
	for (i = 0; i < table->n_segments; i++) {
		LOCK_DISPOSE(table->segment_array[i].lock);
		free((void*)table->segment_array[i].lock);
	}
	free(table->segment_array[0].bucket_array);
	free(table->segment_array);
	free(table);

	return;
}

unsigned int hs_count(hs_table_t *table) 
{
	unsigned int i;
	unsigned int bucket_count = 0;
	for (i = 0; i < table->n_segments; i++) {
		bucket_count += table->segment_array[i].bucket_count;
	}
	return bucket_count;
}


// --------------------------------------------
// PRIVATE FUNCTION DEFENITIONS
// --------------------------------------------

static void find_closer_free_bucket(hs_table_t *table, 
									hs_segment_t *seg, 
									hs_bucket_t **free_bucket, 
									unsigned int *dist_travelled)
{
	unsigned int n_buckets_in_segment = table->n_buckets_in_segment;

	// examine all hop_range-1 preceeding buckets
	hs_bucket_t *current = *free_bucket - (table->hop_range-1);
	if (current < seg->bucket_array) {
		current += n_buckets_in_segment;
	}
	unsigned int bucket_idx = (table->hop_range - 1);
	while (bucket_idx > 0) {
		// look for a occupied bucket in bitmap of current, whose key can be moved to free_bucket
		// don't move the first bucket.
		int move_distance = -1;
		bitmap_t mask = 1;
		bitmap_t hop_info = current->hop_info >> 1;
		unsigned int i;
		for (i = 1; i < bucket_idx; i++) {
			if (mask & hop_info) {
				move_distance = i;
				break;
			}
			hop_info >>= 1;
		}

		if (move_distance != -1) /* closer bucket found */ {
			hs_bucket_t *new_free_bucket = current + move_distance;
			if (new_free_bucket > seg->last_bucket) {
				new_free_bucket -= n_buckets_in_segment;
			}
			// swap
			current->hop_info |= (1 << bucket_idx);
			current->hop_info &= ~(1 << move_distance);
			(*free_bucket)->data = new_free_bucket->data;
			(*free_bucket)->key = new_free_bucket->key;
			
			seg->timestamp++;

			new_free_bucket->key = NULL;
			new_free_bucket->data = NULL;

			*free_bucket = new_free_bucket;
			*dist_travelled -= (bucket_idx-move_distance);

			return;
		}
		current++;
		if (current > seg->last_bucket) {
			current -= n_buckets_in_segment;
		}
		bucket_idx--;
	}
	*dist_travelled = 0;
	*free_bucket = NULL;
}

static void resize(hs_table_t *table)
{
	// Not implemented

	return;
}

static inline unsigned int get_segment_idx(hs_table_t *table, unsigned int hash) 
{
	unsigned int nbits = LOG2(table->n_segments);
	unsigned int mask = 1;
	unsigned int i;
	for (i = 0; i < nbits; i++) {
		mask |= 1 << i;
	}

	return hash & mask;
}

// find occupied bucket whose bit is set to one, and return if it equals the key we're looking for.
static inline hs_bucket_t *find_key(hs_table_t *table, 
									hs_segment_t *seg, 
									hs_bucket_t *base_bucket, 
									void *key)
{
	bitmap_t hop_info = base_bucket->hop_info;
	hs_bucket_t *current = base_bucket;
	hs_bucket_t *last_bucket = seg->last_bucket;

	while (hop_info > 0) {
		if (hop_info & 1) {
			if (current > last_bucket) {
				current -= table->n_buckets_in_segment;
			}
			if (table->cmp_fn(key, current->key) == 0) {
				return current;
			}
		}
		hop_info >>= 1;
		current++;
	}
	return NULL;
}

// EOF
