
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <iostream>
#include <string>
#include <unordered_map>


typedef struct hashel_s
{
    int val;
} hashel_t;

static uint32_t
hash_cb(const void *el)
{
    hashel_t *e = (hashel_t *)el;
    return (uint32_t)e->val;
}

static bool
eq_cb(const void *el1, const void *el2)
{
    hashel_t *e1 = (hashel_t *)el1;
    hashel_t *e2 = (hashel_t *)el2;
    return e1->val == e2->val;
}

/**
 * @see http://c-faq.com/lib/randrange.html
 * @return Random int value in [low, high].
 */
static int
myrand(int low, int high)
{
    int r = low + (rand() / ((RAND_MAX / (high - low + 1)) + 1));
    assert((low <= r && r <= high) && "Invalid random number generated");
    return r;
}

int
main(void)
{
    const int SEED = (int)time(NULL);
    // Previously this seed caused a segfault.
    // const int SEED = 1545011861;

    printf("SEED: %d\n", SEED);

    srand(SEED);
    
    const int max_iter = 1024*1024;
    const int max_len = 1024;
    int len = max_len;
    hashel_t *els = (hashel_t *)calloc(len, sizeof(hashel_t));
    int step = myrand(1, 100000);
    int lowval = myrand(-1000000, 0);
    int nextval = lowval;

    // Generate random values.
    int i;
    for (i = 0; i < len; ++i)
    {
        hashel_t *el = &els[i];
        el->val = nextval;
        nextval = nextval + myrand(1, step);
    }

    // Shuffle our array.
    int tmp;
    int swap;
    for (i = 0; i < len; ++i)
    {
        swap = myrand(0, len - 1);
        if (swap != i)
        {
            tmp = els[i].val;
            els[i].val = els[swap].val;
            els[swap].val = tmp;
        }
    }

    printf("Passed generating random elements to insert\n");

    struct timespec start;
    struct timespec end;

    std::unordered_map<int, bool> u;

    if (clock_gettime(CLOCK_REALTIME, &start))
    {
        printf("Error getting start time: %d, %s\n", errno, strerror(errno));
        return errno;
    }

    int j;
    for (j = 0; j < max_iter; ++j)
    {
        int k;
        for (k = 0; k < len; ++k)
        {
            hashel_t *el = &els[k];
            u.insert({ el->val, true });
        }
        u.clear();
    }

    if (clock_gettime(CLOCK_REALTIME, &end))
    {
        printf("Error getting end time: %d, %s\n", errno, strerror(errno));
        return errno;
    }

    double seconds = (double)(end.tv_sec - start.tv_sec) + ((double)end.tv_nsec - (double)start.tv_nsec)/1000000000;

    int totallen = len * max_iter;
    printf("Passed inserting [%d] items in [%f] seconds or [%f per second]\n", totallen, seconds, ((double)totallen/seconds));

    return 0;
}

