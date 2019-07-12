
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

namespace hashit
{
    class edge_hash
    {
    public:
        size_t
        operator()(const int& k) const
        {
            if (k < 512)
            {
                return k;
            }
            else
            {
                return 3;
            }
        }
    };
}

static constexpr int BLOCK_LEN = crj::detail::BLOCK_LEN;
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
        bool pass_invariant = map.invariant();
        if (!pass_invariant)
        {
            map.print(cout);
        }
        assert(pass_invariant && "Fail: invariant");

        cout << "PASSED SIMPLE LINEAR INSERTION TEST" << endl;
    }

    {
        // Test edge cases with insertion.
        crj::detail::unordered_map<100, int, bool, hashit::edge_hash> map(0, hashit::edge_hash{});

        assert(map.hash_function()(511) == 511 && "Fail: hash function check");

        // Fill the map with direct hits.
        for (int i = 0; i < 512; ++i)
        {
            //cout << "Emplacing: " << i << endl;
            map.emplace(i, true);
        }
        assert(map.size() == 512 && "Fail: map size not 512");
        assert(map.bucket_count() == 512 && "Fail: map length not 512");

        map.erase(3);// Where large numbers get inserted.
        map.erase(5);
        map.emplace(513, false);
        map.emplace(514, false);
        // Force long leap.
        map.erase(500);
        map.emplace(515, false);
        // Force wrap around.
        map.erase(0);
        map.emplace(516, false);
        // Force insert into middle with extended leap.
        map.erase(100);
        map.emplace(517, false);
        // Force insert into middle with normal leap.
        map.erase(300);
        map.emplace(518, false);
        // Erase our head.
        map.erase(513);
        // Erase element with large leap.
        map.erase(517);
        // Erase remaining elements.
        map.erase(518);
        map.erase(516);
        map.erase(515);
        map.erase(514);

        bool pass_invariant = map.invariant(&cout);
        if (!pass_invariant)
        {
            map.print(cout);
        }
        assert(pass_invariant && "Fail: invariant");

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
        // int forceseed = 1559537524;
        int forceseed = FORCESEED;
        int *n = rand_intarr_new(len, &seed, forceseed);
        cout << "SEED: " << seed << endl;

        size_t size = 0;
        int i;
        for (i = 0; i < len; ++i)
        {
            int k = n[i];

            auto p = map.insert({k, true});
            assert(p.second && "Fail: insert unique");

            if (!map.invariant(&cout))
            {
                map.print();
                exit(1);
            }

            p = map.insert({k, true});
            assert(!p.second && "Fail: insert exist");

            assert(1 == map.count(k) && "Fail: contains");

            ++size;
            assert(size == map.size() && "Fail: size");
        }

        assert(map.invariant() && "Fail: invariant");

#if 0
        for (i = 0; i < len; ++i)
        {
            int k = n[i];
            assert(1 == map.erase(k) && "Fail: erase exist");
            assert(0 == map.erase(k) && "Fail: erase nonexist");
        }

        assert(map.invariant() && "Fail: invariant");
#endif

        rand_intarr_free(n);

        cout << "PASSED RANDOM INSERT/ERASE TEST" << endl;
    }

    return 0;
}

