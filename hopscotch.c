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

// This implementation:
// Bitmaps for fast retrieval.
// Assuming little-endianess on architecture, closest bucket (on right side) in bitmap is lsb.
// Locking at segment granularity for any update function.
// Timestamps updated by find_closer_neighbor() and used by hs_get()
// Segments wrap at end of array
// Assumes power of two <n_segments> and <n_buckets_in_segment>.
// See README & [Herlihy et al., 2008] for more details.
// hashed key, using 0 as "magic number", this could cause an error

#include "hopscotch.h"
#include "sync.h"
#include "alloc.h"

typedef unsigned int uint;

typedef struct {
	void *data;
	hash_t hkey;
	bitmap_t hop_info;
} hs_bucket_t;

typedef struct {
	hs_bucket_t *bucket_array;
	hs_bucket_t *last_bucket;
	lock_t *lock;
	uint count;
	volatile uint timestamp;
	uint bucket_count;
} hs_segment_t;

struct hs_table_s {
	hs_segment_t *segment_array;

	uint n_buckets_in_segment;
	uint n_segments;
	//	uint max_segments;

	uint hop_range;
	uint add_range;
	uint max_tries;
};


// --------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
// --------------------------------------------

static hash_t get_segment_idx(hs_table_t *table, hash_t hkey);
static void resize(hs_table_t *table);
static void find_closer_free_bucket (hs_table_t *table, 
									 hs_segment_t *seg, 
									 hs_bucket_t **free_bucket, 
									 uint *dist_travelled);
static inline hs_bucket_t *check_neighborhood(hs_table_t *table, 
											  hs_segment_t *seg, 
											  hs_bucket_t *start_bucket, 
											  hash_t hkey);

// --------------------------------------------
// PUBLIC FUNCTION DEFENITIONS
// --------------------------------------------

hs_table_t *hs_new(uint n_segments,
				   uint n_buckets_in_segment,
				   uint hop_range, 
				   uint add_range,
				   uint max_tries)
{
	hs_table_t *new_table = ALLOCATE(sizeof(hs_table_t));

	new_table->n_segments = n_segments;
	new_table->n_buckets_in_segment = n_buckets_in_segment;
	new_table->hop_range = hop_range;
	new_table->add_range = add_range;
	new_table->max_tries = max_tries;

	new_table->segment_array = ALLOCATE(n_segments*sizeof(hs_segment_t));

	uint i;
	for (i = 0; i < n_segments; i++) {
		hs_segment_t *seg = &(new_table->segment_array[i]);
		seg->lock = malloc(sizeof(pthread_mutex_t));
		LOCK_INIT(seg->lock);
		seg->count = 0;
		seg->timestamp = 0;
		seg->bucket_array = ALLOCATE(n_buckets_in_segment*sizeof(hs_bucket_t));
		seg->last_bucket = &(seg->bucket_array[n_buckets_in_segment-1]);
		seg->bucket_count = 0;
		uint j;
		for (j = 0; j < n_buckets_in_segment; j++) {
			seg->bucket_array[j].hkey = 0;
			seg->bucket_array[j].hop_info = 0;
		}
	}

	return new_table;
}

void hs_put(hs_table_t *table, void *key, void *data)
{
	hash_t hkey = hash_function(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	uint n_buckets_in_segment = table->n_buckets_in_segment;
	hash_t bucket_idx = hkey % n_buckets_in_segment;
	uint hop_range = table->hop_range;
	hs_bucket_t *last_bucket = seg->last_bucket;
	uint add_range = table->add_range;

	hs_bucket_t *start_bucket = &(seg->bucket_array[bucket_idx]);
	LOCK_ACQUIRE(seg->lock);

	// bail out if entry already exists
    hs_bucket_t *bucket_exist = check_neighborhood(table, seg, start_bucket, hkey);
	if (bucket_exist != NULL) {
		LOCK_RELEASE(seg->lock);
		return;
	}

	hs_bucket_t *free_bucket = start_bucket;
	uint dist_travelled;

	// find an empty bucket within ADD_RANGE
	for (dist_travelled = 0; dist_travelled < add_range; dist_travelled++) {
		if (free_bucket->hkey == 0) {
			break;
		}
		free_bucket++;
		if (free_bucket > last_bucket) {
			free_bucket -= n_buckets_in_segment;
		}
	}

	if (dist_travelled < add_range) /* empty bucket found */ {
		do {
			// here we can get info before everything crashes
			if (dist_travelled < hop_range) {
				start_bucket->hop_info |= (1 << dist_travelled);
				free_bucket->hkey = hkey;
				free_bucket->data = data;
				seg->bucket_count++;
				LOCK_RELEASE(seg->lock);
				return;
			}
			find_closer_free_bucket(table, seg, &free_bucket, &dist_travelled);
			// if no bucket found within hop_range, free_bucket is set to NULL and
			// table will be resized.
		} while (free_bucket != NULL);
	}

	LOCK_RELEASE(seg->lock);
	resize(table);
	hs_put(table, key, data);
	return;
}

void *hs_get(hs_table_t *table, void *key)
{
	hash_t hkey = hash_function(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	hash_t bucket_idx = hkey % table->n_buckets_in_segment;
	uint max_tries = table->max_tries;

	hs_bucket_t *start_bucket = &(seg->bucket_array[bucket_idx]);
	hs_bucket_t *check_bucket;

	uint try_counter = 0;
	uint timestamp;

	// Makes <max_tries> attempts if it observes inconsistent state (this can only
	// happen if a hkey is being displaced in same segment).
	do {
		timestamp = seg->timestamp;
		check_bucket = check_neighborhood(table, seg, start_bucket, hkey);
		if (check_bucket != NULL) {
			return check_bucket->data;
		}
		try_counter++;
	} while (try_counter < max_tries && timestamp != seg->timestamp);

	// Consider adding "slow path": Search all [base, base+hop_range] for hkey.
	// Note: slow path would have to consider wrapping of segment.

	return NULL;
}

void *hs_remove(hs_table_t *table,void *key)
{
	hash_t hkey = hash_function(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	hash_t bucket_idx = hkey % table->n_buckets_in_segment;

	hs_bucket_t *start_bucket = &(seg->bucket_array[bucket_idx]);
	hs_bucket_t *check_bucket;
	void *data;

	LOCK_ACQUIRE(seg->lock);	
	check_bucket = check_neighborhood(table, seg, start_bucket, hkey);
	if (check_bucket != NULL) {
		data = check_bucket->data;
		check_bucket->hkey = 0;
		check_bucket->data = NULL;
		start_bucket->hop_info &= ~(1 << (check_bucket - start_bucket));
		seg->bucket_count++;
		return data;
	}
	LOCK_RELEASE(seg->lock);

	return NULL;
}

void hs_destroy(hs_table_t *table)
{
	uint i;
	for (i = 0; i < table->n_segments; i++) {
		LOCK_DISPOSE(table->segment_array[i].lock);
		DEALLOCATE(table->segment_array[i].bucket_array);
	}
	DEALLOCATE(table->segment_array);
	DEALLOCATE(table);

	return;
}

uint hs_sum_bucket_count(hs_table_t *table) 
{
	uint i;
	uint bucket_count = 0;
	for (i = 0; i < table->n_segments; i++) {
		bucket_count += table->segment_array[i].bucket_count;
	}
	return bucket_count;
}


// --------------------------------------------
// PRIVATE FUNCTION DEFENITIONS
// --------------------------------------------

static void find_closer_free_bucket (hs_table_t *table, 
									 hs_segment_t *seg, 
									 hs_bucket_t **free_bucket, 
									 uint *dist_travelled)
{
	uint n_buckets_in_segment = table->n_buckets_in_segment;
	hs_bucket_t *first_bucket = seg->bucket_array;
	hs_bucket_t *last_bucket = seg->last_bucket;

	// examine all hop_range-1 preceeding buckets
	hs_bucket_t *check_bucket = *free_bucket - (table->hop_range-1);
	if (check_bucket < first_bucket) {
		check_bucket += n_buckets_in_segment;
	}
	uint bucket_idx = (table->hop_range - 1);
	while (bucket_idx > 0) {
		// look for a occupied bucket in bitmap of check_bucket, whose key can be moved to free_bucket
		// don't move the first bucket.
		int move_distance = -1;
		hash_t mask = 1;
		bitmap_t hop_info = check_bucket->hop_info >> 1;
		uint i;
		for (i = 1; i < bucket_idx; i++) {
			if (mask & hop_info) {
				move_distance = i;
			}
			hop_info >>= 1;
		}

		if (move_distance != -1) /* closer bucket found */ {
			hs_bucket_t *new_free_bucket = check_bucket + move_distance;
			if (new_free_bucket > last_bucket) {
				new_free_bucket -= n_buckets_in_segment;
			}

			// swap
			check_bucket->hop_info |= (1 << bucket_idx);
			check_bucket->hop_info &= ~(1 << move_distance);
			(*free_bucket)->data = new_free_bucket->data;
			(*free_bucket)->hkey = new_free_bucket->hkey;
			
			seg->timestamp++;

			// this will be set in hs_put if this bucket be used.
			new_free_bucket->hkey = 0;
			new_free_bucket->data = NULL;

			*free_bucket = new_free_bucket;
			*dist_travelled = bucket_idx;

			return;
		}
		check_bucket++;
		if (check_bucket > last_bucket) {
			check_bucket -= n_buckets_in_segment;
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

static hash_t get_segment_idx(hs_table_t *table, hash_t hkey) 
{
	uint nbits = LOG2(table->n_segments);
	hash_t mask = 1;
	uint i;
	for (i = 0; i < nbits; i++) {
		mask |= 1 << i;
	}

	return hkey & mask;
}

// find occupied bucket whose bit is set to one, and return if it equals the key we're looking for.
static inline hs_bucket_t *check_neighborhood(hs_table_t *table, 
											  hs_segment_t *seg, 
											  hs_bucket_t *start_bucket, 
											  hash_t hkey)
{
	bitmap_t hop_info = start_bucket->hop_info;
	hs_bucket_t *this_bucket = start_bucket;
	hs_bucket_t *last_bucket = seg->last_bucket;

	while (hop_info > 0) {
		if (hop_info & 1) {
			if (this_bucket > last_bucket) {
				this_bucket -= table->n_buckets_in_segment;
			}
			if (hkey == this_bucket->hkey) {
				return this_bucket;
			}
		}
		hop_info >>= 1;
		this_bucket++;
	}

	return NULL;
}

// EOF
