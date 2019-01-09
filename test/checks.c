
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"


int
main(void)
{
    int size = (int)sizeof(hashmap_bucket_t);
    
    printf("Bucket Size: %d\n", size);
    assert(size == 64 && "Bucket size is NOT 64 bytes");


#if 0
    /* Just checking that my binary search was working */
    hashmap_t map;
    hashmap_init(&map, -1 * 4, NULL, NULL);
    hashmap_init(&map, 0 * 4, NULL, NULL);
    hashmap_init(&map, 1 * 4, NULL, NULL);
    hashmap_init(&map, 2 * 4, NULL, NULL);
    hashmap_init(&map, 16 * 4, NULL, NULL);
    hashmap_init(&map, 17 * 4, NULL, NULL);
    hashmap_init(&map, 18 * 4, NULL, NULL);
    hashmap_init(&map, 213461828 * 4, NULL, NULL);
    hashmap_init(&map, 418363972 * 4, NULL, NULL);
    hashmap_init(&map, 418363973 * 4, NULL, NULL);
    hashmap_init(&map, 418363974 * 4, NULL, NULL);
#endif

    return 0;
}
