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

#include "hopscotch.h"
#include "hash.h"
#include "sync.h"
#include "alloc.h"


typedef struct {
	hs_segment_t *segment_array;
	hash_function_t hash;

	uint n_buckets_in_segment;
	uint n_segments;
	//	uint max_segments;

	uint hop_range;
	uint add_range;
	uint max_tries;

	key_t segment_mask;
} hs_table_t;

typedef struct {
	hs_bucket_t *bucket_array;
	lock_t lock;
	uint count;
	volatile uint timestamp;
} hs_segment_t;

typedef struct {
	void *data;
	key_t hkey; // hashed key
	bitmap_t hop_info;
} hs_bucket_t;


// --------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
// --------------------------------------------

static key_t get_segment_idx(hs_table_t *table, key_t hkey);
static void hs_resize(hs_table_t *table);
static void find_closer_free_bucket (hs_table_t *table, hs_bucket_t **free_bucket, uint *dist_travelled);
static inline hs_bucket_t *check_neighborhood(hs_table_t *table, hs_bucket_t *start_bucket, key_t hkey);

// Precondition: n must be power of two.
#define MOD(a, n) (a & (n-1))


// --------------------------------------------
// PUBLIC FUNCTION DEFENITIONS
// --------------------------------------------

hs_table_t *hs_new(uint n_segments,
				   uint n_buckets_in_segment,
				   uint hop_range, 
				   uint add_range,
				   uint max_tries,
				   hash_function_t hash)
{
	hs_table_t *new_table = ALLOCATE(sizeof(hs_table_t));
	new_table->segment_array = ALLOCATE(n_segments*sizeof(hs_segment_t));

	uint i;
	for (i = 0; i < n_segments; i++) {
		hs_segment_t seg = new_table->segment_array[i];
		LOCK_INIT(seg.lock);
		seg.count = 0;
		seg.timestamp = 0;
		seg.bucket_array = ALLOCATE(n_buckets_in_segment*sizeof(hs_bucket_t));
		seg.last_bucket = &(seg.bucket_array[n_buckets_in_segment-1]);
		uint j;
		for (j = 0; j < n_buckets_in_segment; j++) {
			seg.bucket_array[j].hkey = NULL;
			seg.bucket_array[j].hop_info = 0;
		}
	}

	new_table->n_segments = n_segments;
	new_table->n_buckets_in_segment = n_buckets_in_segment;
	new_table->hop_range = hop_range;
	new_table->add_range = add_range;
	new_table->max_tries = max_tries;
	new_table->hash = hash;

	new_table->segment_mask = get_segment_mask(n_segments);

	return new_table;
}

// - Starting at i = h(key), use linear probing to find an empty entry at index j.
// - If the empty entry's index j is within H - 1 of i, place x there and return.
// - Otherwise, j is too far from i. To create an empty entry closer to i, find an
//   item y whose hash value lies between i and j, but within H-1 of j, and whose
//   entry lies below j. Displacing y to j creates a new empty slot closer to i.
//   Repeat. If no such item exists, or if the bucket already i contains H items,
//   resize and rehash the table.
void hs_put(hs_table_t *table, void *key, void *data)
{
	key_t hkey = calculate_hash(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	key_t bucket_idx = hkey % table->n_buckets_in_segment;

	hs_bucket_t *start_bucket = seg->bucket_array[bucket_idx];
	LOCK_ACQUIRE(seg->lock);

	// bail out if entry already exists
    hs_bucket_t *existing_bucket = check_neighborhood(table, start_bucket, hkey);
	if (existing_bucket != NULL) {
		LOCK_RELEASE(seg->lock);
		return;
	}

	hs_bucket_t *free_bucket = start_bucket;
	uint dist_travelled;

	for (dist_travelled = 0; dist_travelled < ADD_RANGE; dist_travelled++) {
		if (free_bucket->hkey == NULL) {
			break;
		}
		free_bucket++;
	}

	if (dist_travelled < ADD_RANGE) {
		do {
			if (dist_travelled < table->hop_range) {
				// travelling from msb to lsb
				start_bucket->hop_info |= (1 << (table->hop_range - dist_travelled));
				free_bucket->hkey = hkey;
				free_bucket->data = data;
				LOCK_RELEASE(seg->lock);
				return;
			}
			find_closer_free_bucket(table, free_bucket, dist_travelled);
			// can we guarantee that find_closer_free_bucket() will always find a closer 
			// bucket? If not, then this could be a infinite loop.
		} while (free_bucket != NULL);
	}

	// did not find bucket within ADD_RANGE.
	LOCK_RELEASE(seg->lock);
	resize();
	hs_put(table, key, data);
	return;
}

void *hs_get(hs_table_t *table, void *key);
{
	key_t hkey = calculate_hash(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	key_t bucket_idx = hkey % table->n_buckets_in_segment;

	hs_bucket_t *start_bucket = seg->bucket_array[bucket_idx];
	hs_bucket_t *check_bucket;

	uint try_counter = 0;
	uint timestamp;

	// Makes <max_tries> attempts if it observes inconsistent state (this can only
	// happen if a hkey is being displaced in same segment).
	do {
		timestamp = seg->timestamp;
		check_bucket = check_neighborhood(table, start_bucket, hkey);
		if (check_bucket != NULL) {
			return check_bucket->data;
		}
		try_counter++;
	} while (try_counter < table->max_tries && timestamp != seg->timestamp);

	// Consider adding slow path: Search all [base, base+hop_range] for hkey.

	returne NULL;
}

void *hs_remove(hs_table_t *table,void *key);
{
	key_t hkey = calculate_hash(key);
	hs_segment_t *seg = &(table->segment_array[get_segment_idx(table, hkey)]);
	key_t bucket_idx = hkey % table->n_buckets_in_segment;

	hs_bucket_t *start_bucket = seg->bucket_array[bucket_idx];
	hs_bucket_t *check_bucket;
	void *data;

	LOCK_ACQUIRE(seg->lock);	
	check_bucket = check_neighborhood(table, start_bucket, hkey);
	if (check_bucket != NULL) {
		data = check_bucket->data;
		check_bucket->hkey = NULL;
		check_bucket->data = NULL;
		start_bucket->hop_info &= ~(1 << (check_bucket - start_bucket))
		return data;
	}
	LOCK_RELEASE(seg->lock);

	return NULL;
}

void hs_dispose_table(hs_table_t *table);
{
	uint i;
	for (i = 0; i < n_segments; i++) {
		LOCK_DISPOSE(table->segment_array[i].lock);
		DEALLOCATE(table->segment_array[i].bucket_array);
	}
	DEALLOCATE(table->segment_array);
	DEALLOCATE(table);

	return;
}


// --------------------------------------------
// PRIVATE FUNCTION DEFENITIONS
// --------------------------------------------

static void find_closer_free_bucket (hs_table_t *table, hs_bucket_t *free_bucket, uint *dist_travelled)
{
	hs_bucket_t *bucket check_bucket = free_bucket - (table->hop_range-1);
	uint bucket_idx;
	// examine all hop_range-1 preceeding buckets
	for (bucket_idx = (table->hop_range - 1); bucket_idx > 0; bucket_idx--) {
		int move_distance = -1;
//		key_t mask = ~((key_t)-1 >> 1);
		// look for a occupied bucket in bitmap of check_bucket, whose key can be moved to free_bucket
		uint i;
		for (i = 0; i < bucket_idx; i++
	}

	// within H-1 range(don't want to move a bucket out of the range of its base), find a bucket Y which is not a base bucket. Start at this_bucket_location-(H-1) for efficiency.
	// swap free_bucket with Y
	// update bitmap of Y's base bucket
}

static void hs_resize(hs_table_t *table)
{
	// Not implemented

	return;
}

static key_t get_segment_idx(hs_table_t *table, key_t hkey) {
	uint nbits = (sizeof(key_t)*8 - log2(table->n_segments));
	key_t mask = 0;
	uint i;
	for (i = 0; i < nbits; ++i) {
		mask |= 1 << i;
	}
	return mask;

	return hkey & mask;
}

static inline hs_bucket_t *check_neighborhood(hs_table_t *table, hs_bucket_t *start_bucket, key_t hkey)
{
	hop_info = start_bucket->hop_info;
	hs_bucket_t *this_bucket = start_bucket;

	while (hop_info > 0) {
		if (GET_BMAP_MSB(hop_info) == 0) /* no leading 0-bits from msb */ { 
			if (hkey == this_bucket->hkey) {
				return this_bucket;
			}
		}
		hop_info = hop_info << 1;
		this_bucket++;
	}

	return NULL;
}

// EOF
