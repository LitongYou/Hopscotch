// General purpose 32-bit hash function released under public domain.

#ifndef __HASH_H_
#define __HASH_H_

#define HASH_INITIAL 0

#define HASH_NEXT_CHAR(_hash,_ch) (_ch + (_hash << 6) + (_hash << 16) - _hash)

#define HASH_FINAL(_hash) (hash & 0x7FFFFFFF)

#endif
