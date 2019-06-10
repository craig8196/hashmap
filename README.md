
# hashmap
C hashmap implementation.

## TODO
* [X] Implement insert, get.
* [X] Refactor code.
* [X] Retest.
* [X] Implement remove.
* [X] Implement extended emplace algorithm.
* [X] Retest.
* [X] Refactor code.
* [X] Profile.
* [ ] Implement grow table split.
* [ ] Create large test.
* [ ] Profile.
* [ ] Add FORCESEED DEFINE.
* [ ] Optimize resize iteration.
* [ ] Create speed test for random insert/remove operations.

## Inspiration
Descriptions of Google's hashmap and Malte Skarupke's bytell hashmap
found here:
https://www.youtube.com/watch?v=M2fKMP47slQ

## Design Choices
* Fibonacci Hashing for speed.
* Power of 2 sized tables for ease of growth and modulo.
* Internally will be a table until a fixed point, then a table of tables.
* Open addressing with chaining.
* Two (2) octets of overhead per entry.
    * First octet is 7 bits of hash, or EMPTY value.
    * Second octet is for chaining 1 head, 1 search flag, 6 bits for leap index.
* Densely packed table (load factor of 0.9375, or technically more possibly).

## Fibonacci Hashing
Optimization for clamping to a range of power of 2 sized table (without all negative affects of clustering).
So the trick is to take the range you want to map to: 2**32 for uint32_t
Divide by the Golden Ratio, which splits up the range giving you
a gap to apply between each member.
Note that the implementation casts to uint64_t and wraps around the upper bits.
Convert to index with bit shift or mask.

Example:
```
uint32_t original_hash = n
uint32_t gap = floor(2**32 / GR) = 2654435769
uint32_t gap_prime = 2654435761 OR 2654435789
uint32_t newhash = gap_prime * original_hash
```

## Notes
* From initial tests I found that too much time is spent swapping entries due
  to the robin hood algorithm.
  The algorithm still helps keep data co-located, but data starts getting
  very clustered making data get swapped more and more.
  If several Robinhood runs are next to eachother, then numerous swaps
  need to take place for every insertion.
  Robinhood still is fast upon lookups, but slower than desired on insertion.
  Chaining should be a more effective method for collision resolution,
  up to one (1) relocation per insertion which is better than the cascades
  that can occur with Robinhood.
* From other people's experimentation and research there is possibly
  larger cost with calling the equality callback.
  Thus, part of the hash is kept for a quick lint check.
  Originally I tried storing the full hash, but this is a waste of space.
  Using 7 bits gives ~1/128 chance of false positive.
* When hash tables get large they stop being in cache.
  Thus, the dual design of switching to multiple tables when the table
  gets larger.
  This should keep reallocations smaller/granular and increase the speed of the
  table by keeping it in cache longer.
  So far this doesn't add much, if any, cost.
* From testing I discovered that with my design the linked list must
  grow in one direction only.
  Otherwise you could encounter an infinite loop.
* This implementation appears to be competitive on the Intel platform.
  Unfortunately, performance degrades on AMD machines and barely beats
  std::unordered_map.

## Implementation and Algorithm
* A table is power of two number of slots.
* Each slot is 16 entries since SSE2 instructions take 16 octets for their ops.
* Length of table is number of slots * 16.
* Each slot has a hash block where 7 bits of hash or EMPTY are stored.
* Each slot has a leap block where 8 bits are used to describe linked list.
* Each slot has a entry block to store keys/values.

### First Step
Compute the hash of the key and lookup which table to use.

### NEXT Algorithm
asdf
1. If the leap value is direct/local, then add value to index and return
1. Else the leap value specifies where to start searching:
    1. Set flag indicating subhash of entry cannot be trusted
    1. Use leap value as the slot index offset
    1. Compute subhash of hash
    1. Start LOOP
        1. Search slot for subhash
        1. For each hit compute the original head index from the entry's hash
        1. If the original head index is the same as this list's head index,
           then return index
        1. Else increment slot index
        1. Go to LOOP

### GET Algorithm
1. Find slot and entry
1. If EMPTY, return nothing
1. If LINK, then return nothing
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        * If keys are equal, then return value
    1. If end of list, then return nothing
    1. Compute NEXT entry
    1. Go to LOOP

### INSERT Algorithm
1. Find slot and entry
1. If EMPTY, place key/val and return
1. If LINK, move entry to end of its list, place key/val and return
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        1. If keys are equal, then return EXIST
    1. If end of list
        1. Place key/val and return
    1. Compute NEXT entry
    1. Go to LOOP

### REMOVE Algorithm
1. Find slot and entry
1. If EMPTY, return NOEXIST
1. If LINK, return NOEXIST
1. Entry is HEAD
1. Compute subhash
1. Start LOOP
    1. If entry subhash is equal and can trust subhash
        1. If keys are equal
            1. Copy out key and value
            1. If HEAD
                1. If HEAD is TAIL, mark as EMPTY and return
                1. Else unlink and empty next entry and move to HEAD
            1. Else unlink entry mark as EMPTY and return
    1. If end of list, return NOEXIST
    1. Compute NEXT entry
    1. Go to LOOP

## Invariants
* Every item in a linked list must originally index to the head of the list.
  This is a test for list membership.
* Every subhash must match the hash of the associated entry,
  EXCEPT when the entry is found during an extended search
  (then the entry inherits the subhash from the parent).
* Each entry of a linked list must come after the previous entry.
  This guarantees we do NOT have cycles with the extended search option.
* Linked lists may NOT wrap around the array past the original HEAD.
  This guarantees we do NOT have cycles with the extended search option.
* The size of the table must equal the sum of the list lengths.
* The number of EMPTY values must equal table length minus the size.

## Testing
See `test` directory for available tests.

## Build Examples
Build with different optimizations:
```bash
make all ops=3 # Will set flag -O3
```

Run test:
```bash
make test target=simple # Name of test file is simple.c
```

Run a test with alternate hashmap:
```bash
./acquire.sh # Download Malte Skarupke's hashmap variations
make test target=insertspeed compiler=g++ hashmap=UNORDERED_MAP
# Possible hashmaps:
# UNORDERED_MAP
# UNORDERED_MAP_FIB
# BYTELL_HASH_MAP
# FLAT_HASH_MAP
```

Run a test with profiling and a specific target:
```bash
make test prof=coverage target=insertspeed # See out/index.html
```

Run with gprof:
```bash
make test prof=gprof target=insertspeed
```

