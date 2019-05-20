
# hashmap
C hashmap implementation.

## TODO
* [ ] Implement insert, remove, get.
* [ ] Previous profiling indicates too much swapping, which is hindering performance.
* [ ] Fix bug with inserting conflict with map->swaptmp on the vanilla version.
* [ ] Compare with other hashmap implementations.
* [ ] Allow shrinkage. Develop a heuristic for deallocation. Make an option? Probably &lt;= 25% or 12.5% full is good for shrinkage.

## Design Choices
* Fibonacci Hashing
* Power of 2 sized tables
* Use two indexes to map to entry
* Open addressing with chaining
* Store 7 bits of hash to prevent checking equivalence, 1 for empty
* Store 1 bit for head/tail, 1 bit for linear sweep, 6 bits for leap index
* Incremental rehashing by having array of tables once size gets large
* Densely packed table

## Fibonacci Hashing
Optimization for clamping to a range of power of 2 sized table (without all negative affects of clustering).
So the trick is to take the range you want to map to: 2**32 for uint32_t
Divide by the Golden Ratio, which splits up the range giving you
a gap to apply between each member.

Example:
```
uint32_t original_hash = n
uint32_t gap = floor(2**32 / GR) = 2654435769
uint32_t gap_prime = 2654435761 OR 2654435789
uint32_t newhash = gap_prime * original_hash
```

Use newhash for indexing by using bit shift/mask.

## Notes
1. From initial tests I found that too much time is spent swapping entries due
   to the robin hood algorithm.
   The algorithm still helps keep data co-located, but data starts getting
   very clustered making data get swapped more and more.
   If several Robinhood runs are next to eachother, then numerous swaps
   need to take place upon insertion.
   Robinhood still is fast upon lookups, but slightly slower than linear
   collision resolution on insertion.
   Chaining should be a more effective method to collision resolution.
1. From other people experimentation and research there is a larger cost
   with calling the equality callback.
   Thus, part of the hash is kept for a quick lint check.
1. When hash tables get large they stop being in cache.
   Thus, the dual design of switching to multiple tables when the table
   gets larger.
   This should keep reallocations smaller/granular and increase the speed of the
   table by keeping it in cache.

## Testing
See `test` directory.


## Builds
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

