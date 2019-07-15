
# hashmap
C++ hashmap implementation.

## About
Originally, I wanted to create a hashmap in C that used Robinhood Hashing.
I wasn't happy with my original ideas, design, and performance.
In short, my early attempts sucked.
I researched what others have attempted and created a design based on multiple
ideas.
The algorithm turned out to be difficult to implement, but I think I've found
all of the bugs using code-coverage reports and randomized testing.
In the end I think I've created a competitive hashmap.
I think that this map will perform better than others just because
it can better overcome some worst-case scenarios.
Basically, I consider the design to be more robust.

### When to use
* If you don't need reference stability.
* If you don't have complete confidence in your hash function.
* If you need a hashmap.

### WARNING
If your hash function returns values that don't mixup the upper bits, wrap
your hash function with fibonacci_hash provided by the namespace.
Otherwise the map won't function as efficiently.

### Contributing
Add your name somewhere and submit a pull request :)
The following are additional items that can be worked on:
* [ ] Peer review
* [ ] Optimizations
* [ ] Badges (everybody seems to have them)
* [ ] Documentation
* [ ] Statistics and performance reports

## Inspiration
Descriptions of Google's hashmap and Malte Skarupke's bytell hashmap
found here:  
https://www.youtube.com/watch?v=M2fKMP47slQ
https://github.com/skarupke/flat_hash_map
I really liked the idea of creating a chained hashmap using a single byte.
Chained maps get the benefit of Robinhood Hashing
without some of the down-sides.
I wouldn't really use the flat hashmap or bytell hashmaps just because
I don't trust any hash functions that much in production code.
Basically, if you have too many keys hashing to the same block these maps
could over-allocate or throw errors.

Ideas for optimization from Martin Ankerl's hashmap found here:  
https://github.com/martinus/robin-hood-hashing/
This hashmap is really great, a very clever implementation of robinhood hashing.
The design lends itself to amazing optimizations.
I wouldn't use this map either for much the same reasons as the other
maps above.
What's worse is that if your data starts clustering too much it acts just like
having too many keys hash to the same location.
Additionally, performance can degrade just because one bucket pushes data
down and degrades other buckets by a similar amount.
However, I was not able to compete with the raw speed of this hashmap given
reasonable data.

## Design Choices
* Fibonacci Hashing for speed and to mitigate power of 2 modulo issues.
* Power of 2 sized tables for ease of growth and modulo.
* Open addressing with chaining by linking existing entries.
* Two (2) octets of overhead per entry.
    * First octet is 6 bits of hash, EMPTY value, and head/link bit.
    * Second octet is for chaining, highest value indicates a slower search.
* Densely packed table (load factor of 0.99 default).
* Dense table should mitigate extra memory uses.
* Addition of sentinel byte(s) at end for iterators.

## Fibonacci Hashing
Prime-sized tables are slower, power of 2 is fast but increases collision rate.
Fibonacci Hashing is an optimization for clamping
to a range of power of 2 sized table
(without all negative affects of clustering).
So the trick is to take the range you want to map to: 2**32 for uint32_t
Divide by the Golden Ratio, which splits up the range giving you
a gap to apply between each member.
Convert to index with bit shift or mask.

Example:
```
uint32_t original_hash = n
uint32_t gap = floor(2**32 / GR) = 2654435769
uint32_t gap_prime = 2654435761 OR 2654435789
uint32_t newhash = gap_prime * original_hash
```

## Notes
* I found that my first implementations were slow because I was storing
  too much information (full hash? unnecessary).
  Using too much space drastically decreases performance since you
  start to thrash your cache.
* From initial tests I found that too much time is spent moving entries due
  to the robin hood algorithm.
  The algorithm still helps keep data co-located, but data starts getting
  very clustered making data get swapped more and more.
  If several Robinhood runs are next to eachother, then numerous swaps
  need to take place for every insertion.
  Robinhood still is fast upon lookups, but slower than desired on insertion.
  Chaining should be a more effective method for collision resolution,
  up to one (1) relocation per insertion which is better than the cascades
  that can occur with Robinhood when the table is more full.
* From other people's experimentation and research there is possibly
  larger cost with calling the equality function.
  Thus, part of the hash is kept for a quick lint check.
  Originally I tried storing the full hash, but this is a waste of space.
* When hash tables get large they stop being in cache.
  My implementation uses two bytes of overhead, but tries to make up for it
  with higher load factors.
* From testing I discovered that, with my design, the linked list must
  grow in one direction only.
  Otherwise you could encounter an infinite loop with how the extended
  search algorithm works.
* A pure C implementation takes performance hits due to the lack of additional
  inlining when creating a generic hashmap.
* Performance can differ drastically depending on the platform.
  Originally I ran some simple performance tests on an Intel CPU and they looked
  good.
  However, the gaps and flaws in performance became more evident on an AMD
  machine.
  Perhaps Intel has better pipelining or caching, which could hide some defects.

## Implementation and Algorithm
* The table is power of two number of blocks.
* Each block is 16 entries since SSE2 instructions take 16 octets for their ops.
* Length of table is number of blocks * 16.
* Each block has array of hashes where 6 bits of hash, 1 bit for head/link,
  or EMPTY are stored.
* Each block has array of leaps where 8 bits are used to describe linked list.
* Each block has a entry to store keys/values.

### First Step
Compute the hash of the key and create index.

### NEXT Algorithm
1. If the leap value is direct/local, then add value to index and return
    1. Set flag indicating subhash of entry CAN be trusted
1. Else the leap value specifies where to start searching:
    1. Set flag indicating subhash of entry CANNOT be trusted
    1. Use leap value as the index offset
    1. Get leaping-from entry's subhash and set link flag.
    1. Start LOOP
        1. Search block for subhash
        1. For each hit compute the original head index from the entry's hash
        1. If the original head index is the same as this list's head index,
           then return index
        1. Else increment block index
        1. Go to LOOP

### GET Algorithm
1. Find block and entry
1. If EMPTY, return nothing
1. If LINK, then return nothing
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        * If keys are equal, then return value
    1. If end of list, then return nothing
    1. Set subhash link flag
    1. Compute NEXT entry
    1. Go to LOOP

### INSERT Algorithm
1. Find block and entry
1. If EMPTY, place key/val and return
1. If LINK, move entry to end of its list, place key/val and return
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        1. If keys are equal, then return EXIST
    1. If end of list
        1. Place key/val and return
    1. Set subhash link flag
    1. Compute NEXT entry
    1. Go to LOOP

### REMOVE Algorithm
1. Find block and entry
1. If EMPTY, return NOEXIST
1. If LINK, return NOEXIST
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        1. If keys are equal
            1. Remove key/value
            1. If HEAD
                1. If HEAD is TAIL, mark as EMPTY and return
                1. Else move TAIL (marka as EMPTY) to HEAD and return
            1. Else unlink entry mark as EMPTY and return
    1. If end of list, return NOEXIST
    1. Set subhash link flag
    1. Compute NEXT entry
    1. Go to LOOP

## Invariants
* Every item in a linked list must originally index to the head of the list.
  This is a test for list membership.
* Every subhash must match the hash of the associated entry,
  EXCEPT when the entry is found during an extended search
  (then the entry inherits the subhash from the previous entry).
* Each entry of a linked list must come after the previous entry.
  This guarantees we do NOT have cycles with the extended search option.
* Linked lists may NOT wrap around the array past the original HEAD.
  This guarantees we do NOT have cycles with the extended search option.
* The size of the map must equal the sum of the list lengths.
* The size of the map must equal the sum of non-EMPTY entries.

## Testing
See `test` directory for available tests.

## Build Examples
Build with different optimizations:
```bash
make all ops=3 # Will set flag -O3
```

Run test:
```bash
make test target=prove # Name of test file is prove.cpp
# target=prove is optional (set by default)
```

Run a test with alternate hashmap:
```bash
./acquire.sh # Download hashmap variations
make test target=perform hashmap=UNORDERED_MAP seed=<some number>
# Possible hashmaps:
# HASHMAP (default)
# UNORDERED_MAP
# UNORDERED_MAP_FIB
# BYTELL_HASH_MAP
# FLAT_HASH_MAP
# ROBINHOOD
```

Run a test with profiling and a specific target:
```bash
make test prof=coverage target=prove # See out/index.html
```

Run with gprof:
```bash
make test prof=gprof target=perform
```

