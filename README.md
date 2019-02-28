
# hashmap
A hashmap implementation in C.

## Design Choices
 * Open addressing.
 * Prime size table.
 * Buckets that are cache aligned to 64 bytes on 64 bit intel arch.
   Also, buckets of a fixed size should give some good loop unrolling.
 * Storage of hash and "debt" in attempt to prevent additional computations.
 * Max probe of floor(log2(table_size)) before realloc.
 * Robin hooding.

## Testing
Currently there are four tests.
The first test, simple.c, was just testing a single insert and remove operation.
The second, linear.c, just tests inserting 1024 integers and removing them.
The third, nospace.c, reduces the max size of the map so we can test the no
space return code.
The fourth, random.c, implements random testing to vet and remove remaining flaws.
The fifth, large.c, implements random testing using millions of entries.
The sixth, speed_insert.c, runs a simple performance test for insertions.

Run with different optimizations:
```
make all ops=3 # Will set flag -O3
```

Run the nospace test:
```
make test testtarget=nospace nospace=true
```

Run a test with profiling and a specific target:
```
make test prof=true testtarget=speed_insert2
```

## Further Work
 * [ ] Compare with other hashmap implementations.
 * [ ] Allow shrinkage. Develop a heuristic for deallocation. Make an option? Probably &lt; 25% full is good for reallocation.
 * [ ] Is it possible to prefetch next bucket? So while performing an operation 
   we're fetching the next bucket in the mean-time.
 * [ ] Optimize removal (refactore back shift operation).
 * [ ] Optimize insertion. If possible.

