
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"
#include "util.h"

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


int
main(void)
{
    const int max_iter = 1024*2;
    //const int max_len = 1024*1024;
    const int max_len = 32000;

    int nlen = max_len + max_iter;
    int seed = 0;
    int forceseed = 482530486;
    int *n = rand_intarr_new(nlen, &seed, forceseed);

    printf("SEED: %d\n", seed);

    printf("Passed generating random elements to insert\n");

    int i = 32;
    for (i = 13; i < 8092; i += 1000)
    //for (i = 1; i < max_len; i = (i * 2))
    {
        struct timespec start;
        struct timespec end;

        if (clock_gettime(CLOCK_REALTIME, &start))
        {
            printf("Error getting start time: %d, %s\n", errno, strerror(errno));
            return errno;
        }

        int *nums = n;
        int j;
        for (j = 0; j < max_iter; ++j)
        {
#ifdef HASHMAP
            bool bval = true;
            hashmap_t map;
            hashmap_init(&map, sizeof(n[0]), sizeof(bool), int_hash_cb, int_eq_cb);
#endif
#ifdef UNORDERED_MAP
            std::unordered_map<int, bool> u;
#endif
#ifdef BYTELL_HASH_MAP
            ska::bytell_hash_map<int, bool> u;
#endif
#ifdef FLAT_HASH_MAP
            ska::flat_hash_map<int, bool> u;
#endif
#ifdef UNORDERED_MAP_FIB
            ska::unordered_map<int, bool> u;
#endif

            nums = &n[j];
            int k;
            for (k = 0; k < i; ++k)
            {
#ifdef HASHMAP
                hashmap_insert(&map, &nums[k], &bval);
#endif
#ifdef UNORDERED_MAP
                u.insert({ nums[k], true });
#endif
#ifdef BYTELL_HASH_MAP
                u.insert({ nums[k], true });
#endif
#ifdef FLAT_HASH_MAP
                u.insert({ nums[k], true });
#endif
#ifdef UNORDERED_MAP_FIB
                u.insert({ nums[k], true });
#endif
            }
#ifdef HASHMAP
            hashmap_destroy(&map);
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

        if (clock_gettime(CLOCK_REALTIME, &end))
        {
            printf("Error getting end time: %d, %s\n", errno, strerror(errno));
            return errno;
        }

        double seconds = (double)(end.tv_sec - start.tv_sec) + ((double)end.tv_nsec - (double)start.tv_nsec)/1000000000;

        int totallen = i * max_iter;
        printf("Passed inserting [%d] items [%d] times in [%f] seconds or [%f per second/%f nsec per op]\n", i, max_iter, seconds, ((double)totallen/seconds), (seconds*1000000000.0)/(double)totallen);
    }

    return 0;
}

