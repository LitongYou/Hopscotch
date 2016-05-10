* Hopscotch Hashing

This is an implementation of Hopscotch Hashing described in Herlihy et al. (2008).

Hopscotch hashing combines the advantages of Cuckoo Hashing, Chained Hashing, and Linear probing. The main idea is as follows: There is a "base bucket" that is the bucket for all keys that falls on this index based on the hashed key. When key's collide, they are put as close to this bucket as possible (within a set range), and thus the algorithm utilizes the cached hierarchy of modern computers. All buckets contain a bitmap, which indicates which key's belongs to the bucket, this allows for faster retrieval and removal.

Here's how to add an item (as described in the original paper):
To add an item x where h(x) = i
- Start at i, use linear probing to find an empty entry at index j.
- If the empty entry's index j is within H - 1 of i, place x there and return.
- Otherwise, j is too far from i and j, but within H - 1 of j, and whose entry lies below j. Displacing y to j creates a new empty sot closer to i. Repeat. If no such item exists, or if bucket i already contains H items, resize and rehash the table.

This particular implementation is based on a bitmap (as opposed to linked list).
The hash map is split into segments.
For update functions (i.e. add/remove) locking happens on segment granularity. Thus the number of segments determines the concurrency level.
Get operations uses timestamps (which are updated when displacement happens) and through this, the system is linearizable.

Assumptions:
- Little endian architecture
- Power of two <nbuckets> and <nsegments>

Todo:
- Fix wrapping
- Testing
- Benchmark
- Implement resize()
- Add testing
- Error handling