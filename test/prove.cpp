
#include <string>
#include <assert.h>
#include <stdio.h>
#include <iostream>

#include "util.h"

#include "hackmap.hpp"


using namespace std;

namespace abc
{
    class myhash
    {
    public:
        size_t
        operator()(const int& k) const
        {
            return k * 3;
        }
    };
}

using map_type = crj::unordered_map<int, bool>;

int
main(void)
{

    {
        // Simple initialize/empty test.
        map_type map;

        assert(map.empty() && "Fail: empty");
        assert(0 == map.size() && "Fail: size 0");
        assert(map.find(3) == map.end() && "Fail: find !exist");

        cout << "PASSED INITIALIZE/EMPTY\n";
    }

    {
        // Simple insert and find test.
        // Test that we can initialize the hashmap.

        map_type map;

        size_t count = map.erase(3);
        assert(0 == count && "Fail: map is empty");
        assert(map.size() == 0 && "Fail: map size not 0");

        auto result = map.insert({3, true});
        assert(result.second && "Fail: insert new");

        auto insert_iterator = result.first;
        result = map.insert({3, false});
        assert(!result.second && "Fail: insert exist");

        auto find_iterator = map.find(3);
        assert(find_iterator == insert_iterator && "Fail: find value");
        assert(find_iterator->second && "Fail: value check");

        auto emplace_result = map.emplace(3, false);
        assert(!emplace_result.second && "Fail: emplace exist");

        auto emplace_iterator = emplace_result.first;
        assert(emplace_iterator != map.end() && "Fail: emplace");

        find_iterator = map.find(3);
        assert(!find_iterator->second && "Fail: value update");
        assert(map.size() == 1 && "Fail: map size");

        count = map.erase(4);
        assert(0 == count && "Fail: should not erase anything");
        assert(map.size() == 1 && "Fail: map size changed");

        count = map.erase(3);
        assert(1 == count && "Fail: should erase");
        assert(map.size() == 0 && "Fail: map size not 0");

        cout << "PASSED SIMPLE INSERT/EMPLACE/FIND/ERASE\n";
    }

#if 0
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
        for (i = 0; i < 10000; ++i)
        {
            int el = i;
            int *key = &el;

            hashcode_t c = hashmap_insert(map, key, NULL);
            if (c)
            {
                printf("Code: %d\n", c);
            }
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            if (code != HASHCODE_EXIST)
            {
                printf("Code: %d\n", code);
            }
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        hashmap_invariant(map);
        // Stats from this look good.
        hashmap_print_stats(map);
        hashmap_destroy(map);
    }

    {
        // Simple linear multiple of 8 insert.
        hashmap_init(map, sizeof(int), 0, int_hash_cb, int_eq_cb);
        int size = 0;
        int i;
        for (i = 0; i < 40000; ++i)
        {
            int el = i * 8;
            int *key = &el;

            hashcode_t c = hashmap_insert(map, key, NULL);
            if (c)
            {
                hashmap_print_stats(map);
                hashmap_invariant(map);
                printf("Code: %d\n", c);
            }
            assert(HASHCODE_OK == c && "Failed insert");

            hashcode_t code = hashmap_insert(map, key, NULL);
            if (code != HASHCODE_EXIST)
            {
                printf("Code: %d\n", code);
            }
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        hashmap_invariant(map);
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
            if (code != HASHCODE_EXIST)
            {
                printf("Code: %d\n", (int)code);
            }
            assert(HASHCODE_EXIST == code && "Failed reinsert");

            assert(hashmap_contains(map, key) && "Failed contains");

            ++size;
            assert(size == hashmap_size(map) && "Failed size 1");
        }
        hashmap_invariant(map);
        // Stats from this look good.
        hashmap_print_stats(map);
        hashmap_destroy(map);

        rand_intarr_free(n);
    }
#endif

    return 0;
}

