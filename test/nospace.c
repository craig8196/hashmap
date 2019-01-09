/**
 * @brief Tests that the correct code is returned if the map is full.
 * @note This test reduces the max size of the table using macros.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"


static uint32_t
intptr_hash_cb(const void *el)
{
    intptr_t x = (intptr_t)el;
    return (uint32_t)x;
}

static bool
intptr_eq_cb(const void *el1, const void *el2)
{
    return el1 == el2;
}

#define MAX_PRIME (11)

#ifndef HASHMAP_BUCKET_COUNT
#define HASHMAP_BUCKET_COUNT (4)
#endif

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, intptr_hash_cb, intptr_eq_cb);

    int size = 0;
    intptr_t i;
    for (i = 0; i < MAX_PRIME * HASHMAP_BUCKET_COUNT; ++i)
    {
        assert(HASHCODE_OK == hashmap_insert(&map, (void *)i, NULL) && "Failed insert test");
        assert(HASHCODE_EXIST == hashmap_insert(&map, (void *)i, NULL) && "Failed double insert test");
        size++;
        assert(hashmap_contains(&map, (void *)i) && "Failed contains test");
        assert(size == hashmap_size(&map) && "Failed size test");
    }
    printf("%s\n", "Passed inserting max number amount");

    assert(HASHCODE_NOSPACE == hashmap_insert(&map, (void *)i, NULL) && "Failed full insert test");
    printf("%s\n", "Passed hashmap too full test");

    hashmap_destroy(&map);

    return 0;
}

