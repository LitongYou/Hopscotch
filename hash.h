// General purpose 32-bit hash function released under public domain.

#ifndef __HASH_H_
#define __HASH_H_

#define HASH_INITIAL 0

#define HASH_NEXT_CHAR(_hash,_ch) (_ch + (_hash << 6) + (_hash << 16) - _hash)

#define HASH_FINAL(_hash) (hash & 0x7FFFFFFF)

#include <stdlib.h>

inline uint32_t calculate_hash(char *str) {
	hash = HASH_INITIAL;
	int i = 0;
	while (str[i] != 0) {
		hash = HASH_NEXT_CHAR(hash, str[i]);
		i++;
	}
	return HASH_FINAL(hash);
}

#endif

// For other functions see:
// http://stackoverflow.com/questions/7666509/hash-function-for-string
// Probably better use CityHash
