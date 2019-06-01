
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

    {
        // Simple empty tests.
        // Test that we can initialize the hashmap.
        hashmap_init(map, sizeof(int), 0, hash_cb, eq_cb);
        assert(hashmap_empty(map) && "Failed empty");
        assert(0 == hashmap_size(map) && "Failed zero");
        hashmap_destroy(map);
    }

    {
        // Simple insert one test.
        // Test that we can insert into empty cell.
        hashmap_init(map, sizeof(int), 0, hash_cb, eq_cb);
        int el = 1;
        int *key = &el;
        assert(HASHCODE_OK == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(HASHCODE_EXIST == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(1 == hashmap_size(map) && "Failed size 1");
        assert(!hashmap_empty(map) && "Failed empty");
        assert(NULL != hashmap_get(map, key) && "Failed get");
        hashmap_destroy(map);
    }

    {
        // Simple insert with bad hash.
        // Test that one item is properly chained onto the next.
        hashmap_init(map, sizeof(int), 0, badhash_cb, badeq_cb);
        int el = 1;
        int *key = &el;
        assert(HASHCODE_OK == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(HASHCODE_EXIST == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(hashmap_contains(map, key) && "Failed contains");
        assert(1 == hashmap_size(map) && "Failed size 1");
        el = 2;
        assert(HASHCODE_OK == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(HASHCODE_EXIST == hashmap_insert(map, key, NULL) && "Failed insert");
        assert(hashmap_contains(map, key) && "Failed contains");
        assert(2 == hashmap_size(map) && "Failed size 1");
        hashmap_destroy(map);
    }

    {
        // Simple insert with bad hash with linear problems.
        // Test that one item is properly chained onto the next even when
        // the large jump is needed.
        hashmap_init(map, sizeof(int), 0, hash_cb, eq_cb);

#if 0
        static const int vals[] =
        {
            //1, 2, 3, 5, 8, 13, 
            //8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 65536
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
        };
        static const int len = sizeof(vals)/sizeof(vals[0]);
#endif
        int size = 0;
        int i;
        for (i = 0; i < 1024; ++i)
        {
            int el = i;
            int *key = &el;

            hashcode_t c = hashmap_insert(map, key, NULL);
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        hashmap_destroy(map);
    }

    return 0;
}

