
# hashmap
A hashmap implementation in C.

## Design Choices
 * Open addressing.
 * Prime size table.
 * Buckets that are cache aligned to 64 bytes on 64 bit intel arch.
   Also, buckets of a fixed size should give some good loop unrolling.
 * Storage of hash and "debt" in attempt to prevent additional computations.
 * Max probe of floor(log2(maxsize)) * 2 before realloc. May not be best choice.
 * Robin hooding.

## Testing
Currently there are four tests.
The first test, simple.c, was just testing a single insert and remove operation.
The second, linear.c, just tests inserting 1024 integers and removing them.
The third, nospace.c, reduces the max size of the map so we can test the no
space return code.
The fourth, random.c, implements random testing to vet and remove remaining flaws.

## Further Work
 * Validate that the Robin Hood algorithms are implemented correctly.
 * Revisit max probe calculations and heuristic for growing the map.
 * Allow shrinkage. Develop a heuristic for deallocation. Make an option?
 * Run performance tests.
 * Compare with other hashmap implementations.
 * Is it possible to prefetch next bucket? So while performing an operation 
   we're fetching the next bucket in the mean-time.
 * Optimize removal (refactore back shift operation).
 * Optimize insertion. If possible.

