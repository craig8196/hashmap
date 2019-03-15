
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"


static uint32_t
int_hash_cb(const void *key)
{
    return (uint32_t)(*((int *)key));
}

static bool
int_eq_cb(const void *key1, const void *key2)
{
    int k1 = *((int *)key1);
    int k2 = *((int *)key2);
    return k1 == k2;
}

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, sizeof(int), 0, int_hash_cb, int_eq_cb);

    int x = 13;
    int xout = 0;

    assert(hashmap_is_empty(&map) && "Failed to be empty");
    printf("%s\n", "Passed empty");
    assert(0 == hashmap_insert(&map, &x, NULL, false) && "Failed to insert");
    printf("%s\n", "Passed insert one");
    assert(hashmap_contains(&map, &x) && "Failed simple contains one value");
    printf("%s\n", "Passed contains one");
    assert(1 == hashmap_size(&map) && "Failed size is one");
    printf("%s\n", "Passed size one");
    assert(0 == hashmap_remove(&map, &x, &xout, NULL) && "Failed to remove");
    printf("%s\n", "Passed remove one");
    assert(x == xout && "Failed to save value from hashmap");
    printf("%s\n", "Passed value out one");
    assert(hashmap_is_empty(&map) && "Failed is empty test");
    printf("%s\n", "Passed empty of one");

    hashmap_destroy(&map);

    return 0;
}

