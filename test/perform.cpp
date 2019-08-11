
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

#ifndef FORCESEED
#define FORCESEED (0)
#endif

#ifndef MAXITER
#define MAXITER (100)
#endif

#ifndef MAXLEN
#define MAXLEN (500000)
#endif

#ifdef HASHMAP
#include "hackmap.hpp"
#else

#ifdef UNORDERED_MAP
#include <unordered_map>
#else

#ifdef BYTELL_HASH_MAP
#include "bytell_hash_map.hpp"
#else

#ifdef FLAT_HASH_MAP
#include "flat_hash_map.hpp"
#else

#ifdef UNORDERED_MAP_FIB
#include "unordered_map.hpp"
#else

#ifdef ROBINHOOD
#include "robin_hood.hpp"
#else

#include <unordered_map>

#endif
#endif
#endif
#endif
#endif
#endif

using namespace std;

typedef enum state_e
{
    STATE_OUT = 0,
    STATE_IN  = 1,
} state_t;

typedef enum action_e
{
    ACTION_HAS = 0, // Contains.
    ACTION_INS = 1, // Insert.
    ACTION_DEL = 2, // Delete/Remove.
} action_t;

#define MAX_ACTIONS (5)

typedef struct entry_s
{
    int val;
    state_t state;
    action_t actions[MAX_ACTIONS];
} entry_t;

#ifdef HASHMAP
template class hackmap::unordered_map<int, bool>;
using map_type = hackmap::unordered_map<int, bool>;
#else

#ifdef UNORDERED_MAP
using map_type = std::unordered_map<int, bool>;
#else

#ifdef BYTELL_HASH_MAP
using map_type = ska::bytell_hash_map<int, bool>;
#else

#ifdef FLAT_HASH_MAP
using map_type = ska::flat_hash_map<int, bool>;
#else

#ifdef UNORDERED_MAP_FIB
using map_type = ska::unordered_map<int, bool>;
#else

#ifdef ROBINHOOD
using map_type = robin_hood::unordered_map<int, bool>;
#else

using map_type = std::unordered_map<int, bool>;

#endif
#endif
#endif
#endif
#endif
#endif



static void
runtest(entry_t *e, int elen, int maxactions)
{
    map_type m;

#ifdef DEBUG
    for (int eIndex = 0; eIndex < elen; ++eIndex)
    {
        e[eIndex].state = STATE_OUT;
    }
    size_t size = 0;
#endif

    int action;
    for (action = 0; action < maxactions; ++action)
    {
        int i;
        for (i = 0; i < elen; ++i)
        {
            entry_t *el = &e[i];
#ifndef DEBUG
            switch (el->actions[action])
            {
                case ACTION_HAS:
                    {
                        m.find(el->val);
                    }
                    break;
                case ACTION_INS:
                    {
                        m.insert({ el->val, true });
                    }
                    break;
                case ACTION_DEL:
                    {
                        m.erase(el->val);
                    }
                    break;

            }
#else
            int key = el->val;
            switch (el->actions[action])
            {
                case ACTION_HAS:
                    {
                        cout << "F: " << key << endl;
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
                        cout << "I: " << key << endl;
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
                        cout << "D: " << key << endl;
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
#endif
        }
    }
}

static int
advance_runlength(int prev, int *counter, int *factor)
{
    if (0 == ((*counter) % 10))
    {
        (*factor) = (*factor) * 10;
    }
    ++(*counter);
    return prev + (*factor);
}

int
main(void)
{
    // seed = 1560313389;
    // seed = 1560374140;
    int seed = FORCESEED;
    int forceseed = FORCESEED;
    const int maxiter = MAXITER;
    const int maxlen = MAXLEN;

    int nlen = maxlen + maxiter;
    int *n = rand_intarr_new(nlen, &seed, forceseed);
    printf("SEED: %d\n", seed);

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

    printf("# Done generating random elements.\n");
    printf("# Format:\n"
           "# len = number of elements per iteration\n"
           "# iter = number of iterations\n"
           "# actionlen = number of actions per element\n"
           "# ops = total over all runs [ins, ins exist, erase, erase exist, find, find exist]\n"
           "# seconds = number of seconds\n");

    int runlength;
    int counter = 1;
    int factor = 1;
    for (runlength = 1; runlength < maxlen; runlength = advance_runlength(runlength, &counter, &factor))
    {
        struct timespec start;
        struct timespec end;

        if (clock_gettime(CLOCK_REALTIME, &start))
        {
            printf("Error getting start time: %d, %s\n", errno, strerror(errno));
            return errno;
        }

        int iterindex;
        for (iterindex = 0; iterindex < maxiter; ++iterindex)
        {
            runtest(&e[iterindex], runlength, MAX_ACTIONS);
        }

        if (clock_gettime(CLOCK_REALTIME, &end))
        {
            printf("Error getting end time: %d, %s\n", errno, strerror(errno));
            return errno;
        }


        double seconds =
            (double)(end.tv_sec - start.tv_sec) 
            + ((double)end.tv_nsec - (double)start.tv_nsec)/1000000000.0;

        printf("{\"len\":%d,\"iter\":%d,\"actionlen\":%d,\"ops\":[%d,%d,%d,%d,%d,%d],\"seconds\":%f}\n",
               runlength, maxiter, MAX_ACTIONS, 0, 0, 0, 0, 0, 0, seconds);
    }

    free(e);

    return 0;
}

