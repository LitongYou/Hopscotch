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
