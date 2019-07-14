
#include <string>
#include <assert.h>
#include <stdio.h>
#include <iostream>

#include "util.h"

#include "hackmap.hpp"

#ifndef FORCESEED
#define FORCESEED (0)
#endif

#ifndef MAXITER
#define MAXITER (1)
#endif

#ifndef MAXLEN
#define MAXLEN (1024)
#endif


using namespace std;

typedef enum state_e
{
    STATE_OUT = 0,
    STATE_IN  = 1,
} state_t;

typedef enum action_e
{
    ACTION_HAS = 0, // Has/Contain.
    ACTION_INS = 1, // Insert/Emplace/Put/Upsert.
    ACTION_DEL = 2, // Delete/Remove/Erase.
} action_t;

#define MAX_ACTIONS (5)

typedef struct entry_s
{
    int val;
    state_t state;
    action_t actions[MAX_ACTIONS];
} entry_t;

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

template class crj::detail::unordered_map<100, int, bool>;
using map_full_type = crj::detail::unordered_map<100, int, bool>;

int
main(void)
{

#if 1
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
        map.reserve(BLOCK_LEN);

        for (int i = 0; i < BLOCK_LEN; ++i)
        {
            map.emplace(i, true);
            assert(1 == map.count(i) && "Fail: not in map");
        }
        assert(map.size() == crj::detail::BLOCK_LEN && "Fail: wrong size");
        assert(map.invariant(&cout) && "Fail: invariant");

        cout << "PASSED SIMPLE LINEAR INSERTION TEST" << endl;
    }

    {
        // Test edge cases with insertion.
        map_edge_type map(0, hashit::edge_hash{});

        assert(map.invariant(&cout) && "Fail: invariant");

        map.clear();

        size_t min_max_size = size_t(1) << ((sizeof(size_t) * 8) - 2);
        assert(map.max_size() >= min_max_size && "Fail: max_size");

        assert(map.key_eq()(1, 1) && "Fail: key equal");
        assert(map.key_eq()(EDGEMAX, EDGEMAX) && "Fail: key equal");
        assert(!map.key_eq()(1, 2) && "Fail: key unequal");
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

        // Clear data.
        map.clear();

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
        
        // Remove head with extended leaps.
        map.insert({8, true});
        map.erase(500);
        map.erase(1000);
        map.insert({b + 2, false});
        map.insert({b + 3, false});
        assert(map.invariant(&cout) && "Fail: invariant");
        map.erase(b + 1);
        assert(map.invariant(&cout) && "Fail: invariant");
        map.erase(b + 3);
        assert(map.invariant(&cout) && "Fail: invariant");
        map.erase(b + 2);
        assert(map.invariant(&cout) && "Fail: invariant");

        // Reset data.
        map.reset();

        // Reinsert everything.
        for (int i = 0; i < EDGEMAX; ++i)
        {
            map.insert({i, true});
        }

        // Case of cascade break when finding that next leap is local.
        map.erase(3);
        map.erase(500);
        map.erase(600);
        map.erase(605);
        map.insert({b + 1, false});
        map.insert({b + 2, false});
        map.insert({b + 3, false});
        map.insert({b + 4, false});
        map.erase(b + 1);
        assert(map.invariant(&cout) && "Fail: invariant");

        // Reset data.
        map.reset();

        // Reinsert everything.
        for (int i = 0; i < EDGEMAX; ++i)
        {
            map.insert({i, true});
        }

        // Case of inserting when link inhabits the spot.
        map.erase(3);
        map.erase(500);
        map.erase(600);
        map.erase(605);
        map.erase(606);
        map.insert({b + 1, false});
        map.insert({b + 2, false});
        map.insert({b + 3, false});
        map.insert({500, true});
        assert(map.invariant(&cout) && "Fail: invariant");
        map.erase(500);
        map.erase(b + 2);
        map.erase(b + 3);
        map.insert({b + 2, false});
        map.insert({b + 3, false});
        map.insert({b + 4, false});
        map.insert({600, true});
        assert(map.invariant(&cout) && "Fail: invariant");

        // Equal range tests.
        using it_type = map_edge_type::iterator;
        using cit_type = map_edge_type::const_iterator;
        pair<it_type, it_type> r1 = map.equal_range(98);
        assert(r1.first != map.end() && r1.second != map.end() && "Fail: equal_range");
        pair<cit_type, cit_type> r2 = map.equal_range(398);
        assert(r2.first != map.cend() && r2.second != map.cend() && "Fail: equal_range");
        pair<cit_type, cit_type> r3 = map.equal_range(-1);
        assert(r3.first == map.cend() && r3.second == map.cend() && "Fail: equal_range");

        // Bucket size.
        assert(0 == map.bucket(0) && "Fail: bucket");
        assert(1 == map.bucket_size(0) && "Fail: bucket size");
        map.erase(0);
        assert(map.bucket_count() == map.bucket(0) && "Fail: bucket");
        assert(0 == map.bucket_size(0) && "Fail: bucket size");
        assert(map.cend() == map.end() && "Fail: iterator comparison");
        assert(map.cbegin() == map.begin() && "Fail: iterator comparison");

        map.emplace(0, true);
        map.emplace(1, true);

        // Swap
        map_edge_type map1;
        map_edge_type map2;

        map1.emplace(1, false);
        map1.swap(map1);
        assert(1 == map1.size() && "Fail: self swap");
        assert(1 == map1.count(1) && "Fail: self swap");
        assert(0 == map2.size() && "Fail: self swap");
        assert(0 == map2.count(1) && "Fail: self swap");
        map2.swap(map1);
        assert(0 == map1.size() && "Fail: self swap");
        assert(0 == map1.count(1) && "Fail: self swap");
        assert(1 == map2.size() && "Fail: self swap");
        assert(1 == map2.count(1) && "Fail: self swap");
        map1.swap(map2);
        assert(1 == map1.size() && "Fail: self swap");
        assert(1 == map1.count(1) && "Fail: self swap");
        assert(0 == map2.size() && "Fail: self swap");
        assert(0 == map2.count(1) && "Fail: self swap");
        
        // Test operator[]
        map[0] = false; // true from above
        const int key = 1;
        map[key] = false;
        assert(!map[0] && !map[key] && "Fail: set test");

        cout << "PASSED BASIC EDGE CASE TEST" << endl;
    }

    {
        // Test rehash
        map_edge_type map;

        for (int i = 0; i < BLOCK_LEN; ++i)
        {
            map.insert({i, true});
        }
        map.insert({BLOCK_LEN + 1, false});
        assert(map.bucket_count() > BLOCK_LEN && "Fail: len <= BLOCK_LEN");
        map.erase(BLOCK_LEN + 1);
        map.rehash(BLOCK_LEN);
        assert(map.bucket_count() == BLOCK_LEN && "Fail: rehash/downsizing");

        cout << "PASSED REHASH TEST" << endl;
    }

    {
        // Test at().
        map_edge_type map;

        for (int i = 0; i < BLOCK_LEN; ++i)
        {
            map.insert({i, true});
        }

        assert(map.at(0) && "Failed: at");
        try
        {
            map.at(BLOCK_LEN + 4);
            assert(false && "Fail: out of range");
        }
        catch (std::out_of_range& e)
        {
            assert(true && "Fail: out of range");
        }

        cout << "PASSED AT TEST" << endl;
    }

    {
        // Test erase iterators.
        map_edge_type map({
            {1, true},
            {2, true},
            {3, true},
            {4, true},
        });

        map.erase(map.find(1));
        assert(map.size() == 3 && "Fail: erase");
        map.erase(map.begin(), map.end());
        assert(map.size() == 0 && "Fail: erase");
    }

    {
        // Test constructors.

        map_edge_type m1({
            {1, true},
            {2, true},
            {3, true},
            {4, true},
        });

        map_edge_type m2(m1);
        std::allocator<std::pair<int, bool>> a;
        map_edge_type m3(a);
        map_edge_type m4(m1, a);

        map_edge_type m5;
        m5 = {
            {1, true},
            {2, true},
            {3, true},
            {5, true},
        };
        m5 = m5;
        m5 = m1;// Test map of same size.

        map_edge_type m6;
        m6 = m1;// Test map of diff size.

        // Test move constructors and move assignment.
        map_edge_type m7;
        m7 = std::move(m1);
        assert(m1.size() == 0 && "Fail: move assign");
        assert(m7.size() == 4 && "Fail: move assign");

        map_edge_type m8(std::move(m7));
        assert(m7.size() == 0 && "Fail: move construct");
        assert(m8.size() == 4 && "Fail: move construct");
        map_edge_type m8a;
        map_edge_type m8b(std::move(m8a));

        map_edge_type m9(std::move(m8), a);
        assert(m8.size() == 0 && "Fail: move construct");
        assert(m9.size() == 4 && "Fail: move construct");
        map_edge_type m9a;
        map_edge_type m9b(std::move(m9a), a);

        // Move assign with no elements.
        map_edge_type m10;
        map_edge_type m11;
        m11 = std::move(m11);
        m11 = std::move(m10);

        cout << "PASSED CONSTRUCTORS TEST" << endl;
    }
#endif

#if 1
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
#endif

#if 1
    {
        // TODO randomize tests to find flaw use 100% filled map
        // Initialize random operations.
        int seed = FORCESEED;
        int forceseed = FORCESEED;
        // forceseed = 1563127243;
        const int maxiter = MAXITER;
        const int maxactions = MAX_ACTIONS;
        const int maxlen = MAXLEN;

        int nlen = maxlen + maxiter;
        int *n = rand_intarr_new(nlen, &seed, forceseed);
        cout << "SEED: " << seed << endl;

        entry_t *e = (entry_t *)malloc(sizeof(entry_t) * nlen);
        int i;
        for (i = 0; i < nlen; ++i)
        {
            e[i].val = n[i];
            e[i].state = STATE_OUT;
            int j;
            for (j = 0; j < MAX_ACTIONS; ++j)
            {
                e[i].actions[j] = (action_t)rand_int_range(ACTION_HAS, ACTION_DEL);
            }
        }

        rand_intarr_free(n);

        cout << "DONE CREATING RANDOM NUMBERS" << endl;

        // Start timer.
        struct timespec start;
        struct timespec end;

        if (clock_gettime(CLOCK_REALTIME, &start))
        {
            printf("Error getting start time: %d, %s\n", errno, strerror(errno));
            return errno;
        }

        // Start the test.
        for (int iterCount = 0; iterCount < maxiter; ++iterCount)
        {
            map_full_type m;
            entry_t *elements = e + iterCount;
            size_t size = 0;
            for (int action = 0; action < maxactions; ++action)
            {
                for (int eIndex = 0; eIndex < maxlen; ++eIndex)
                {
                    entry_t *el = &elements[eIndex];
                    int key = el->val;
                    switch (el->actions[action])
                    {
                        case ACTION_HAS:
                            {
                                if (STATE_OUT == el->state)
                                {
                                    assert(m.find(key) == m.end() && "Fail: no find");
                                }
                                else
                                {
                                    assert(m.find(key) != m.end() && "Fail: find");
                                }
                                assert(size == m.size() && "Fail: size");
                            }
                            break;
                        case ACTION_INS:
                            {
                                if (STATE_OUT == el->state)
                                {
                                    assert(m.emplace(key, true).second && "Fail: add");
                                    ++size;
                                }
                                else
                                {
                                    assert(!m.emplace(key, true).second && "Fail: no add");
                                }
                                el->state = STATE_IN;
                                assert(size == m.size() && "Fail: size");
                                assert(m.find(key) != m.end() && "Fail: find");
                            }
                            break;
                        case ACTION_DEL:
                            {
                                if (STATE_OUT == el->state)
                                {
                                    assert(0 == m.erase(key) && "Fail: no erase");
                                }
                                else
                                {
                                    assert(1 == m.erase(key) && "Fail: no erase");
                                    --size;
                                }
                                el->state = STATE_OUT;
                                assert(size == m.size() && "Fail: size");
                                assert(m.find(key) == m.end() && "Fail: find");
                            }
                            break;

                    }
                }
            }
            
            // Reset state for next round.
            for (int eIndex = 0; eIndex < maxlen; ++eIndex)
            {
                elements[eIndex].state = STATE_OUT;
            }
        }

        if (clock_gettime(CLOCK_REALTIME, &end))
        {
            printf("Error getting end time: %d, %s\n", errno, strerror(errno));
            return errno;
        }

        double seconds =
            (double)(end.tv_sec - start.tv_sec) 
            + ((double)end.tv_nsec - (double)start.tv_nsec)/1000000000.0;

        int runops = maxlen * maxactions;
        int totalops = runops * maxiter;
        printf(
            "Stat: [%d] items [%d] times in "
            "[%f] seconds or [%f per second/%f nsec per op]\n",
            runops,
            maxiter,
            seconds,
            ((double)totalops/seconds),
            (seconds*1000000000.0)/(double)totalops);

        free(e);

        cout << "PASSED RANDOM ACTIONS TEST" << endl;
    }
#endif

    return 0;
}

