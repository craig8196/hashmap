
#include <string>
#include <assert.h>
#include <stdio.h>
#include <iostream>

#include "util.h"

#include "hackmap.hpp"

#ifndef FORCESEED
#define FORCESEED (0)
#endif


using namespace std;

#define EDGEMAX (1024)

namespace hashit
{
    class edge_hash
    {
    public:
        size_t
        operator()(const int& k) const
        {
            size_t h = k;
            if (k >= EDGEMAX)
            {
                h = 3 | (size_t(k) << ((sizeof(size_t) * 8) - 6));
            }

            return h;
        }
    };
}

static constexpr int BLOCK_LEN = crj::detail::BLOCK_LEN;

template class crj::detail::unordered_map<100, int, bool, hashit::edge_hash>;
using map_edge_type = crj::detail::unordered_map<100, int, bool, hashit::edge_hash>;

template class crj::unordered_map<int, bool>;
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

        cout << "PASSED INITIALIZE/EMPTY TEST" << endl;
    }
    
    {
        // Simple hash function test.
        map_type map;

        assert(map.hash_function()(1) != 1 && "Fail: make sure fib hash auto-wraps");

        cout << "PASSED HASHER TEST" << endl;
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

        cout << "PASSED SIMPLE INSERT/EMPLACE/FIND/ERASE TEST" << endl;
    }

    {
        // Test simple linear insertion.
        map_type map;

        for (int i = 0; i < BLOCK_LEN; ++i)
        {
            map.emplace(i, true);
            //map.print(cout);
            assert(1 == map.count(i) && "Fail: not in map");
        }
        assert(map.size() == crj::detail::BLOCK_LEN && "Fail: wrong size");
        bool pass_invariant = map.invariant(&cout);
        if (!pass_invariant)
        {
            map.print(cout);
        }
        assert(pass_invariant && "Fail: invariant");

        cout << "PASSED SIMPLE LINEAR INSERTION TEST" << endl;
    }

    {
        // Test edge cases with insertion.
        map_edge_type map(0, hashit::edge_hash{});

        assert(map.hash_function()(EDGEMAX - 1) == (EDGEMAX - 1) && "Fail: hash function check");

        // Fill the map with direct hits.
        for (int i = 0; i < EDGEMAX; ++i)
        {
            //cout << "Emplacing: " << i << endl;
            map.emplace(i, true);
        }
        assert(map.size() == EDGEMAX && "Fail: map size not 512");
        assert(map.bucket_count() == EDGEMAX && "Fail: map length not 512");

        constexpr int b = EDGEMAX + 5;

        map.erase(3);// Where large numbers get inserted.
        map.erase(5);
        map.emplace(b + 1, false);
        map.emplace(b + 2, false);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Force long leap.
        map.erase(500);
        map.emplace(b + 3, false);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Force wrap around.
        map.erase(0);
        map.emplace(b + 4, false);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Force insert into middle with extended leap.
        map.erase(100);
        map.emplace(b + 5, false);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Force insert into middle with normal leap.
        map.erase(b + 5);
        map.emplace(100, true);
        assert(map.invariant(&cout) && "Fail: invariant");
        map.erase(400);
        map.emplace(b + 5, false);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Erase our head.
        map.erase(b + 1);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Erase element with large leap.
        map.erase(b + 5);
        assert(map.invariant(&cout) && "Fail: invariant");
        // Erase remaining elements.
        map.erase(b + 4);
        map.erase(b + 2);
        map.erase(b + 3);
        assert(map.invariant(&cout) && "Fail: invariant");

        // Reinsert everything.
        for (int i = 0; i < EDGEMAX; ++i)
        {
            map.insert({i, true});
        }

        // Perform upsert on non-head.
        map.erase(3);
        map.erase(8);
        map.insert({b + 1, false});
        map.insert({b + 2, false}); // Tail.
        assert(false == map[b + 2] && "Fail: value is false");
        map.emplace(b + 2, true);
        assert(true == map[b + 2] && "Fail: value is true");

        // Remove non-exist on single entry list (head only).
        map.erase(b + 5);
        map.erase(b + 2);
        map.erase(b + 4);

        // Clear data.
        map.clear();

        cout << "PASSED BASIC EDGE CASE TEST" << endl;
    }

    {
        // Larger linear test.
        map_type map;

        int max = 10000;
        int i;
        for (i = 0; i < max; ++i)
        {
            auto p = map.insert({i, true});
            assert(p.second && "Fail: new item");
            assert(1 == map.count(i) && "Fail: contains");
        }

        assert(map.invariant(&cout) && "Fail: invariant");

        for (i = 0; i < max; ++i)
        {
            assert(1 == map.erase(i) && "Fail: erase");
            assert(0 == map.count(i) && "Fail: not contains");
        }

        assert(map.invariant(&cout) && "Fail: invariant");

        cout << "PASSED LARGER LINEAR TEST" << endl;
    }

    {
        // Larger linear multiple of 8 test.
        map_type map;

        map.reserve(1024);

        int max = 40000;
        int i;
        for (i = 0; i < max; ++i)
        {
            int val = i * 8;
            auto p = map.insert({val, true});
            assert(p.second && "Fail: new item");
            assert(1 == map.count(val) && "Fail: contains");
        }

        assert(map.invariant(&cout) && "Fail: invariant");

        for (i = 0; i < max; ++i)
        {
            int val = i * 8;
            assert(1 == map.erase(val) && "Fail: erase");
            assert(0 == map.count(val) && "Fail: not contains");
        }

        assert(map.invariant(&cout) && "Fail: invariant");

        cout << "PASSED LARGER LINEAR MULTIPLE OF 8 TEST" << endl;
    }

    {
        // Simple random number insertions.
        map_type map;

        // Generate numbers.
        const int len = 10000;
        int seed = 0;
        int forceseed = FORCESEED;
        forceseed = 1562918581;
        int *n = rand_intarr_new(len, &seed, forceseed);
        cout << "SEED: " << seed << endl;

        size_t size = 0;
        int i;
        for (i = 0; i < len; ++i)
        {
            int k = n[i];

            auto p = map.insert({k, true});
            assert(p.second && "Fail: insert unique");

            p = map.insert({k, true});
            assert(!p.second && "Fail: insert exist");

            assert(1 == map.count(k) && "Fail: contains");

            ++size;
            assert(size == map.size() && "Fail: size");
        }

        assert(map.invariant() && "Fail: invariant");

        for (i = 0; i < len; ++i)
        {
            int k = n[i];
            assert(1 == map.erase(k) && "Fail: erase exist");
            assert(0 == map.erase(k) && "Fail: erase nonexist");
        }

        assert(map.invariant() && "Fail: invariant");

        rand_intarr_free(n);

        cout << "PASSED RANDOM INSERT/ERASE TEST" << endl;
    }

    return 0;
}

