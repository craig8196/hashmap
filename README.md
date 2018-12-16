
# hashmap
Attempt at a hashmap implementation in C

## Design choices
 * Open addressing.
 * Prime size table.
 * Buckets that are cache aligned to 64 bytes on 64 bit intel arch.
 * Collision resolution first in bucket.
 * Max probe of floor(log2(maxsize)) * 2 before realloc.
 * Robin hooding.

