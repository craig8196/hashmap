
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"
#include "util.h"


int
main(void)
{
    hashmap_t hmap;
    hashmap_t *map = &hmap;

    {
        // Simple empty tests.
        // Test that we can initialize the hashmap.
        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
        assert(hashmap_empty(map) && "Failed empty");
        assert(0 == hashmap_size(map) && "Failed zero");
        hashmap_destroy(map);
    }

    {
        // Simple insert one test.
        // Test that we can insert into empty cell.
        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
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
        hashmap_init(map, sizeof(int), 0, int_badhash_cb, int_eq_cb);
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
        // Simple linear insert.
        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
        int size = 0;
        int i;
        for (i = 0; i < 40000; ++i)
        {
            int el = i;
            int *key = &el;

            hashcode_t c = hashmap_insert(map, key, NULL);
#if DEBUG
            if (c)
            {
                hashmap_print(map);
                printf("Code: %d\n", c);
            }
#endif
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        // Stats from this look good.
        hashmap_print_stats(map);
        hashmap_destroy(map);
    }

    {
        // Simple linear multiple of 8 insert.
        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
        int size = 0;
        int i;
        for (i = 0; i < 70000; ++i)
        {
            int el = i * 8;
            int *key = &el;

            hashcode_t c = hashmap_insert(map, key, NULL);
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        // Stats from this look good.
        hashmap_print_stats(map);
        hashmap_destroy(map);
    }

    {
        // Simple random number insertions.
        const int len = 10000;
        int seed = 0;
        // int forceseed = 1559537524;
        int forceseed = 0;
        int *n = rand_intarr_new(len, &seed, forceseed);
        printf("SEED: %d\n", seed);

        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
        int size = 0;
        int i;
        for (i = 0; i < len; ++i)
        {
            int *key = &n[i];

            hashcode_t c = hashmap_insert(map, key, NULL);
            if (c)
            {
                printf("Code: %d\n", (int)c);
            }
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        // Stats from this look good.
        hashmap_print_stats(map);
        hashmap_destroy(map);

        rand_intarr_free(n);
    }

    return 0;
}

