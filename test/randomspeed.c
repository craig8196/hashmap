
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"
#include "util.h"

#ifndef FORCESEED
#define FORCESEED (0)
#endif

#ifdef UNORDERED_MAP
#include <unordered_map>
#endif

#ifdef BYTELL_HASH_MAP
#include "bytell_hash_map.hpp"
#endif

#ifdef FLAT_HASH_MAP
#include "flat_hash_map.hpp"
#endif

#ifdef UNORDERED_MAP_FIB
#include "unordered_map.hpp"
#endif

#ifdef ROBINHOOD
#include "robin_hood.hpp"
#endif

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
static int
load_cb(int maxlen)
{
    return (int)((double)maxlen * 0.75);
}
#endif

static void
runtest(entry_t *e, int elen, int maxactions)
{
#ifdef HASHMAP
    bool bval = true;
    hashmap_t hmap;
    hashmap_t *map = &hmap;
    hashmap_init(map, sizeof(e[0].val), sizeof(bool), int_hash_cb, int_eq_cb);
    hashmap_set_load_cb(map, load_cb);
#ifdef ALLOW_RESERVE
    hashmap_reserve(map, elen);
#endif
#endif
#ifdef UNORDERED_MAP
    std::unordered_map<int, bool> u;
#ifdef ALLOW_RESERVE
    u.reserve(elen);
#endif
#endif
#ifdef BYTELL_HASH_MAP
    ska::bytell_hash_map<int, bool> u;
#ifdef ALLOW_RESERVE
    u.reserve(elen);
#endif
#endif
#ifdef FLAT_HASH_MAP
    ska::flat_hash_map<int, bool> u;
#ifdef ALLOW_RESERVE
    u.reserve(elen);
#endif
#endif
#ifdef UNORDERED_MAP_FIB
    ska::unordered_map<int, bool> u;
#ifdef ALLOW_RESERVE
    u.reserve(elen);
#endif
#endif
#ifdef ROBINHOOD
    robin_hood::unordered_map<int, bool> u;
#ifdef ALLOW_RESERVE
    u.reserve(elen);
#endif
#endif

    int action;
    for (action = 0; action < maxactions; ++action)
    {
        int i;
        for (i = 0; i < elen; ++i)
        {
            entry_t *el = &e[i];
            switch (el->actions[action])
            {
                case ACTION_HAS:
                    {
#ifdef HASHMAP
                    hashmap_get(map, &el->val);
#endif
#if defined UNORDERED_MAP || defined BYTELL_HASH_MAP || defined FLAT_HASH_MAP || defined UNORDERED_MAP_FIB || defined ROBINHOOD
                    u.find(el->val);
#endif
                    }
                    break;
                case ACTION_INS:
                    {
#ifdef HASHMAP
                    hashmap_insert(map, &el->val, &bval);
#endif
#if defined UNORDERED_MAP || defined BYTELL_HASH_MAP || defined FLAT_HASH_MAP || defined UNORDERED_MAP_FIB || defined ROBINHOOD
                    u.insert({ el->val, true });
#endif
                    }
                    break;
                case ACTION_DEL:
                    {
#ifdef HASHMAP
                    hashmap_remove(map, &el->val, NULL, NULL);
#endif
#if defined UNORDERED_MAP || defined BYTELL_HASH_MAP || defined FLAT_HASH_MAP || defined UNORDERED_MAP_FIB || defined ROBINHOOD
                    u.erase(el->val);
#endif
                    }
                    break;

            }
        }
    }


#ifdef HASHMAP
    hashmap_destroy(map);
#endif
#ifdef UNORDERED_MAP
    // Auto cleanup
#endif
#ifdef BYTELL_HASH_MAP
    // Auto cleanup
#endif
#ifdef FLAT_HASH_MAP
    // Auto cleanup
#endif
#ifdef UNORDERED_MAP_FIB
    // Auto cleanup
#endif
}

static int
advance_runlength(int prev, int *counter, int *factor)
{
#if 0
    if (0 == ((*counter) % 10))
    {
        (*factor) = (*factor) * 10;
    }
    ++(*counter);
    return prev + (*factor);
#else
    *factor = *factor;
    if ((*counter) / 10)
    {
        ++(*counter);
    }
    return prev + *counter;
#endif


}

int
main(void)
{
    // seed = 1560313389;
    int seed = FORCESEED;
    int forceseed = FORCESEED;
    const int maxiter = 1024;
    const int maxlen = 1 << 13;

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
            //e[i].actions[j] = (action_t)rand_int_range(ACTION_HAS, ACTION_DEL);
            e[i].actions[j] = (action_t)rand_int_range(ACTION_HAS, ACTION_INS);
        }
    }

    printf("Done generating random elements.\n");

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

        int runops = runlength * MAX_ACTIONS;
        int totalops = runops * maxiter;
        printf(
            "Stat: [%d] items [%d] times in "
            "[%f] seconds or [%f per second/%f nsec per op]\n",
            runops,
            maxiter,
            seconds,
            ((double)totalops/seconds),
            (seconds*1000000000.0)/(double)totalops);
    }

    return 0;
}

