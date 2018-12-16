
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"


static uint32_t
intptr_hash_cb(void *el)
{
    intptr_t x = (intptr_t)el;
    return (uint32_t)x;
}

static bool
intptr_eq_cb(void *el1, void *el2)
{
    return el1 == el2;
}

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, intptr_hash_cb, intptr_eq_cb);

    intptr_t x = 13;
    void *out = NULL;

    assert(hashmap_is_empty(&map) && "Failed to be empty");
    printf("%s\n", "Passed empty");
    assert(0 == hashmap_insert(&map, (void *)x, NULL) && "Failed to insert");
    printf("%s\n", "Passed insert one");
    assert(hashmap_contains(&map, (void *)x) && "Failed simple contains one value");
    printf("%s\n", "Passed contains one");
    assert(1 == hashmap_size(&map) && "Failed size is one");
    printf("%s\n", "Passed size one");
    assert(0 == hashmap_remove(&map, (void *)x, &out) && "Failed to remove");
    printf("%s\n", "Passed remove one");
    assert(x == (intptr_t)out && "Failed to save value from hashmap");
    printf("%s\n", "Passed value out one");
    assert(hashmap_is_empty(&map) && "Failed is empty test");
    printf("%s\n", "Passed empty of one");

    hashmap_destroy(&map);

    return 0;
}

