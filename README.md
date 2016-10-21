## Hopscotch Hashing

This is an implementation of Hopscotch Hashing described in Herlihy et al. (2008).


### Implementation:  

  * 'put' locks on segment granularity, 'get' uses a more optimistic approach based on timestamps
  * Bitmap indicates which key belongs to index  


### Example of use:
    
    
          hs_table_t *table;
          table = hs_new(N_SEGMENTS, N_BUCKETS_IN_SEGMENT, HOP_RANGE, ADD_RANGE, MAX_TRIES, &farmhash, &strcmp);
    
          char *key = malloc(size);
          char *val = malloc(size);
          // set key and value
          s = hs_put(table, key, val);
          if (!s) { printf("insert failed\n"); }
    
          val = hs_get(table, "foo"))
    
          hs_remove(table, key);
          free(key);
    
          hs_destroy(table);
    


### Assumptions:  

  * Little endian architecture  
  * Power of two nbuckets and nsegments  


### Todo:  

  * Implement resize()  
  * Support update


### Contact:

mail: cs @ tooein.net