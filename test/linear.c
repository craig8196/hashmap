
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

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, intptr_hash_cb, intptr_eq_cb);

    void *out = NULL;
    const int len = 1024;
    int size = 0;
    intptr_t i;
    for (i = 0; i < len; ++i)
    {
        assert(HASHCODE_OK == hashmap_insert(&map, (void *)i, NULL) && "Failed insert test");
        assert(HASHCODE_EXIST == hashmap_insert(&map, (void *)i, NULL) && "Failed double insert test");
        size++;
        assert(hashmap_contains(&map, (void *)i) && "Failed contains test");
        assert(size == hashmap_size(&map) && "Failed size test");
    }
    printf("%s\n", "Passed linearly inserting numbers");

    for (i = 0; i < len; ++i)
    {
        out = (void *)(intptr_t)-1;
        assert(hashmap_contains(&map, (void *)i) && "Failed contains test");
        assert(HASHCODE_OK == hashmap_remove(&map, (void *)i, &out) && "Failed remove test");
        assert((i == (intptr_t)out) && "Failed store item from remove");
        --size;
        assert(size == hashmap_size(&map) && "Failed size test");
        assert(!hashmap_contains(&map, (void *)i) && "Failed contains test");
    }
    printf("%s\n", "Passed linearly removing numbers");

    hashmap_destroy(&map);

    return 0;
}

