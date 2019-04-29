
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

extern void hashmap_print(hashmap_t *map);

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, sizeof(int), 0, int_hash_cb, int_eq_cb);

    int out = -1;
    const int len = 1024;
    int size = 0;
    int i;
    for (i = 0; i < len; ++i)
    {
        assert(HASHCODE_OK == hashmap_insert(&map, &i, NULL, false) && "Failed insert test");
        assert(HASHCODE_EXIST == hashmap_insert(&map, &i, NULL, false) && "Failed double insert test");
        size++;
        assert(hashmap_contains(&map, &i) && "Failed contains test");
        assert(size == hashmap_size(&map) && "Failed size test");
    }
    printf("%s\n", "Passed linearly inserting numbers");

    for (i = 0; i < len; ++i)
    {
        out = -1;
        printf("Checking %d\n", i);
        assert(hashmap_contains(&map, &i) && "Failed contains test");
        assert(HASHCODE_OK == hashmap_remove(&map, &i, &out, NULL) && "Failed remove test");
        assert((i == out) && "Failed store item from remove");
        --size;
        assert(size == hashmap_size(&map) && "Failed size test");
        assert(!hashmap_contains(&map, &i) && "Failed contains test");
    }
    printf("%s\n", "Passed linearly removing numbers");

    hashmap_destroy(&map);

    return 0;
}

