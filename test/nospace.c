/**
 * @brief Tests that the correct code is returned if the map is full.
 * @note This test reduces the max size of the table using macros.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"


static uint32_t
int_hash_cb(const void *key)
{
    int k = (*((int *)key));
    return (uint32_t)k;
}

static bool
int_eq_cb(const void *key1, const void *key2)
{
    int k1 = *((int *)key1);
    int k2 = *((int *)key2);
    return k1 == k2;
}

#define MAX_PRIME (11)

int
main(void)
{
    hashmap_t map;

    hashmap_init(&map, 0, sizeof(int), 0, int_hash_cb, int_eq_cb);

    int size = 0;
    int i;
    for (i = 0; i < MAX_PRIME; ++i)
    {
        assert(HASHCODE_OK == hashmap_insert(&map, &i, NULL, false) && "Failed insert test");
        assert(HASHCODE_EXIST == hashmap_insert(&map, &i, NULL, false) && "Failed double insert test");
        size++;
        assert(hashmap_contains(&map, &i) && "Failed contains test");
        assert(size == hashmap_size(&map) && "Failed size test");
    }
    printf("%s\n", "Passed inserting max number amount");

    assert(HASHCODE_NOSPACE == hashmap_insert(&map, &i, NULL, false) && "Failed full insert test");
    printf("%s\n", "Passed hashmap too full test");

    hashmap_destroy(&map);

    return 0;
}

