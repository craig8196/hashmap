
# hashmap
A hashmap implementation in C.

## TODO
* [ ] Profiling indicates too much swapping, which is hindering performance.
* [ ] Fix bug with inserting conflict with map->swaptmp on the vanilla version.
* [ ] Compare with other hashmap implementations.
* [ ] Allow shrinkage. Develop a heuristic for deallocation. Make an option? Probably &lt;= 25% full is good for reallocation.

## Design Choices
* Fibonacci Hashing
* Power of 2 sized table
* Open addressing
* Storage of hash
* Size of elements customizable
* Max probe of floor(log2(table_size)) before realloc
* Robin hooding

* Chaining is basically like robin hooding, however, it avoids moving elements.

## Fibonacci Hashing
Optimization for clamping to a range of power of 2 sized table (without all negative affects of clustering).
So the trick is to take the range you want to map to: 2**32 for uint32_t
Divide by the Golden Ratio, which splits up the range.
Then use probably the upper most bits to use to map into the array.
Example:
uint32_t hash = n
uint32_t gap = floor(2**32 / GR) = 2654435769
uint32_t gap_prime = 2654435761 OR 2654435789
gap = gap_prime # primes seem to make things better
int table_size = 1024
int probe = 10
int index = (hash * gap) >> ((sizeof(uint32_t)*8) - probe)
gap can be constant
probe and shift can be precomputed


## Testing
The first test, simple.c, was just testing a single insert and remove operation.
The second, linear.c, just tests inserting 1024 integers and removing them.
The third, nospace.c, reduces the max size of the map so we can test the no
space return code.
The fourth, random.c, implements random testing to vet and remove remaining flaws.
The fifth, large.c, implements random testing using millions of entries.
The sixth, speed_insert.c, runs a simple performance test for insertions.

Run with different optimizations:

```bash
make all ops=3 # Will set flag -O3
```

Run the nospace test:

```bash
make test target=nospace nospace=true
```

Run a test with profiling and a specific target:

```bash
make test prof=coverage target=speed_insert2
```

Check the `out/` directory for index.html to view coverage.

Run with gprof:

```bash
make test prof=gprof target=speed_insert2
```

