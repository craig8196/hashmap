
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"
#include "util.h"


int
main(void)
{
    const int max_iter = 1024 * 100;
    const int max_len = 1024;

    int nlen = max_len + max_iter;
    int seed = 0;
    int forceseed = 0;
    int *n = rand_intarr_new(nlen, &seed, forceseed);

    printf("SEED: %d\n", seed);

    printf("Passed generating random elements to insert\n");

    struct timespec start;
    struct timespec end;

    hashmap_t map;
    hashmap_init(&map, sizeof(n[0]), 0, int_hash_cb, int_eq_cb);

    if (clock_gettime(CLOCK_REALTIME, &start))
    {
        printf("Error getting start time: %d, %s\n", errno, strerror(errno));
        return errno;
    }

    int *nums = n;
    int j;
    for (j = 0; j < max_iter; ++j)
    {
        nums = &n[j];
        int k;
        for (k = 0; k < max_len; ++k)
        {
            hashmap_insert(&map, &nums[k], NULL);
        }
        hashmap_clear(&map);
    }

    if (clock_gettime(CLOCK_REALTIME, &end))
    {
        printf("Error getting end time: %d, %s\n", errno, strerror(errno));
        return errno;
    }

    hashmap_destroy(&map);

    double seconds = (double)(end.tv_sec - start.tv_sec) + ((double)end.tv_nsec - (double)start.tv_nsec)/1000000000;

    int totallen = max_len * max_iter;
    printf("Passed inserting [%d] items in [%f] seconds or [%f per second]\n", totallen, seconds, ((double)totallen/seconds));

    return 0;
}

