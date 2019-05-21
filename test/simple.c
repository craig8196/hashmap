
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"


static uint32_t
hash_cb(const void *key)
{
    return (uint32_t)(*((int *)key));
}

static bool
eq_cb(const void *key1, const void *key2)
{
    int k1 = *((int *)key1);
    int k2 = *((int *)key2);
    return k1 == k2;
}

static uint32_t
badhash_cb(const void *key)
{
    key = key;
    return 1;
}

static bool
badeq_cb(const void *key1, const void *key2)
{
    int k1 = *((int *)key1);
    int k2 = *((int *)key2);
    return k1 == k2;
}

int
main(void)
{
    hashmap_t hmap;
    hashmap_t *map = &hmap;

    // Simple empty tests.
    hashmap_init(map, sizeof(int), 0, hash_cb, eq_cb);
    assert(hashmap_empty(map) && "Failed empty");
    assert(0 == hashmap_size(map) && "Failed zero");
    hashmap_destroy(map);

    // Simple insert one test.
    hashmap_init(map, sizeof(int), 0, hash_cb, eq_cb);
    int el = 1;
    int *key = &el;
    assert(HASHCODE_OK == hashmap_insert(map, key, NULL) && "Failed insert");
    assert(HASHCODE_EXIST == hashmap_insert(map, key, NULL) && "Failed insert");
    assert(1 == hashmap_size(map) && "Failed size 1");
    assert(!hashmap_empty(map) && "Failed empty");
    assert(NULL != hashmap_get(map, key) && "Failed get");
    hashmap_print(map);
    hashmap_destroy(map);

    // Simple insert with bad hash.
    hashmap_init(map, sizeof(int), 0, badhash_cb, badeq_cb);
    hashmap_destroy(map);

    return 0;
}

