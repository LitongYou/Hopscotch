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

#ifndef __HOPSCOTCH_H_
#define __HOPSCOTCH_H_

//#define SPINLOCK
#define MUTEX

#include <stdint.h> // uint32_t in this header.
#include <stddef.h> // size_t in this header.

// gcc built-in
#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))

typedef uint32_t bitmap_t;

// user provide hash function and fixed sized keys of KEYLEN bytes
#define KEYLEN 16
uint64_t (*hash_function)(const char *str, size_t len);

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
//   uint32_t is sufficient. hop_range should consider the size of cache line.
// - User must provide a  hash function  that hashes key type into something of type
//   hash_t, this type can be changed above.
// - max_tries specifies how many times hs_get  will try to search through its range
//   for a consistent state.
hs_table_t *hs_new(unsigned int n_segments,
				   unsigned int n_buckets_in_segment,
				   unsigned int hop_range, 
				   unsigned int add_range,
				   unsigned int max_tries);

void hs_put(hs_table_t *table, void *key, void *data);

void *hs_get(hs_table_t *table, void *key);

// - returns data so it can be free'd on the same level as it was allocated, returns
//   NULL if key wasn't found.
void *hs_remove(hs_table_t *table, void *key);

void hs_destroy(hs_table_t *table);

unsigned int hs_sum_bucket_count(hs_table_t *table);

#endif

// EOF
