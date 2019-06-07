
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"
#include "util.h"


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

#define MAX_ACTIONS (2)

typedef struct entry_s
{
    int val;
    state_t state;
    action_t actions[MAX_ACTIONS];
} entry_t;

void
runtest(hashmap_t *map, int len, int forceseed)
{
    int seed;
    int *n = rand_intarr_new(len, &seed, forceseed);
    entry_t *e = (entry_t *)malloc(sizeof(entry_t) * len);
    printf("SEED: %d\n", seed);

    int i;
    for (i = 0; i < len; ++i)
    {
        e[i].val = n[i];
        e[i].state = STATE_OUT;
        int j;
        for (j = 0; j < MAX_ACTIONS; ++j)
        {
            e[i].actions[j] = rand_int_range(ACTION_HAS, ACTION_DEL);
        }
    }

    printf("%s\n", "Passed generating random elements to insert");

    hashmap_init(map, sizeof(int), sizeof(int), int_hash_cb, int_eq_cb);

    int size = 0;
    int k;
    for (k = 0; k < MAX_ACTIONS; ++k)
    {
        for (i = 0; i < len; ++i)
        {
            entry_t *el = &e[i];

            switch (el->actions[k])
            {
                case ACTION_HAS:
                {
                    const void *key = &el->val;
                    if (STATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(map, key) && "Failed contains test");
                    }
                    else
                    {
                        assert(hashmap_contains(map, key) && "Failed contains test");
                    }
                    assert(size == hashmap_size(map) && "Failed size test");
                }
                break;
                case ACTION_INS:
                {
                    const void *key = &el->val;
                    if (STATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(map, key) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_insert(map, key, key) && "Failed insert not exist");
                        ++size;
                    }
                    else
                    {
                        assert(hashmap_contains(map, key) && "Failed contains test");
                        assert(HASHCODE_EXIST == hashmap_insert(map, key, key) && "Failed insert exists test");
                    }
                    el->state = STATE_IN;
                    assert(size == hashmap_size(map) && "Failed size test");
                    assert(hashmap_contains(map, key) && "Failed contains test");
                }
                break;
                case ACTION_DEL:
                {
                    const void *key = &el->val;
                    int out = -1;
                    if (STATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(map, key) && "Failed contains test");
                        assert(HASHCODE_NOEXIST == hashmap_remove(map, key, NULL, &out) && "Failed remove not exist");
                        assert(-1 == out && "Failed no modify if not in");
                    }
                    else
                    {
                        assert(hashmap_contains(map, key) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_remove(map, key, NULL, &out) && "Failed remove test");
                        assert(el->val == out && "Failed successful remove");
                        --size;
                    }
                    el->state = STATE_OUT;
                    assert(size == hashmap_size(map) && "Failed size test");
                    assert(!hashmap_contains(map, key) && "Failed contains test");
                }
                break;
            }
        }
    }

    hashmap_invariant(map);
    hashmap_print_stats(map);

    hashmap_destroy(map);
}

int
main(void)
{
    int seed = 0;
    
    hashmap_t hmap;
    hashmap_t *map = &hmap;

    int i;
    int numtests = 10;
    for (i = 0; i < numtests; ++i)
    {
        runtest(map, (i * 12000) + 1, seed);
    }

    printf("%s\n", "Passed performing random actions");

    return 0;
}

