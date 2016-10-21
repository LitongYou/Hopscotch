## Hopscotch Hashing

This is an implementation of Hopscotch Hashing described in Herlihy et al. (2008).


### Implementation:  

  * Concurrent  
  * Bitmap indicates which key belongs to index  
  * Locking on segment granularity  


### Assumptions:  

  * Little endian architecture  
  * Power of two nbuckets and nsegments  


### Todo:  

  * Implement resize()  


### Contact:

mail: cs @ tooein.net