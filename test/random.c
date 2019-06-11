
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"
#include "util.h"

#ifndef FORCESEED
#define FORCESEED (0)
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

#define MAX_ACTIONS (25)

typedef struct entry_s
{
    int val;
    state_t state;
    action_t actions[MAX_ACTIONS];
} entry_t;

void
runtest(hashmap_t *map, int len, int forceseed)
{
    hashcode_t code = HASHCODE_OK;
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

    printf("Done generating random elements\n");

    hashmap_init(map, sizeof(int), sizeof(int), int_hash_cb, int_eq_cb);

    int size = 0;
    int k;
#ifdef DEBUG
    bool hasit = false;
#endif
    for (k = 0; k < MAX_ACTIONS; ++k)
    {
        for (i = 0; i < len; ++i)
        {
            entry_t *el = &e[i];

            switch (el->actions[k])
            {
                case ACTION_HAS:
                {
#ifdef DEBUG
                    printf("Has: %d\n", el->val);
#endif
                    const void *key = &el->val;
                    if (STATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(map, key) && "Failed contains test");
                    }
                    else
                    {
                        if (!hashmap_contains(map, key))
                        {
                            printf("Error with val: %d\n", el->val);
                            fflush(stdout);
                        }
                        assert(hashmap_contains(map, key) && "Failed contains test");
                    }
                    assert(size == hashmap_size(map) && "Failed size test");
                }
                break;
                case ACTION_INS:
                {
#ifdef INVARIANT
                    if (2135701705 == el->val)
                    {
                        code = hashmap_invariant(map);
                        if (code)
                        {
                            printf("Error before insert: %d\n", el->val);
                            hashmap_print(map);
                            exit(1);
                        }
                        hashmap_print(map);
                    }
#endif
#ifdef DEBUG
                    printf("Insert: %d\n", el->val);
#endif
                    const void *key = &el->val;
                    if (STATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(map, key) && "Failed contains test");
                        code = hashmap_insert(map, key, key);
                        assert(HASHCODE_OK == code && "Failed insert not exist");
                        ++size;
                    }
                    else
                    {
                        assert(hashmap_contains(map, key) && "Failed contains test");
                        assert(HASHCODE_EXIST == hashmap_insert(map, key, key) && "Failed insert exists test");
                    }
#ifdef INVARIANT
                    if (2135701705 == el->val)
                    {
                        code = hashmap_invariant(map);
                        if (code)
                        {
                            printf("Error after insert: %d\n", el->val);
                            hashmap_print(map);
                            exit(1);
                        }
                    }
#endif
                    el->state = STATE_IN;
                    assert(size == hashmap_size(map) && "Failed size test");
                    assert(hashmap_contains(map, key) && "Failed contains test");
                }
                break;
                case ACTION_DEL:
                {
#ifdef INVARIANT
                    if (2135701705 == el->val)
                    {
                        code = hashmap_invariant(map);
                        if (code)
                        {
                            printf("Error before remove: %d\n", el->val);
                            hashmap_print(map);
                            exit(1);
                        }
                    }
#endif
#ifdef DEBUG
                    if (528865857 == el->val)
                    {
                        hashmap_print(map);
                        hashmap_invariant(map);
                    }
                    printf("Delete: %d\n", el->val);
#endif
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
#ifdef INVARIANT
                    if (2135701705 == el->val)
                    {
                        code = hashmap_invariant(map);
                        if (code)
                        {
                            printf("Error after remove: %d\n", el->val);
                            hashmap_print(map);
                            exit(1);
                        }
                    }
#endif
#ifdef DEBUG
                    if (528865857 == el->val)
                    {
                        hashmap_print(map);
                        hashmap_invariant(map);
                    }
#endif
                    el->state = STATE_OUT;
                    assert(size == hashmap_size(map) && "Failed size test");
                    assert(!hashmap_contains(map, key) && "Failed contains test");
                }
                break;
            }

#ifdef DEBUG
            if (hasit)
            {
                int val = 1845678091;
                if (!hashmap_contains(map, &val))
                {
                    hashmap_print(map);
                    printf("ERROR HAS OCCURRED\n");
                    fflush(stdout);
                    exit(1);
                }
            }
#endif
        }
    }

    hashmap_invariant(map);

#ifdef VERBOSE
    hashmap_print_stats(map);
#endif

    hashmap_destroy(map);

    rand_intarr_free(n);
    free(e);
}

int
main(void)
{
    int seed = FORCESEED;
    //Seeds that have led to bugs:
    //seed = 1559935378;
    //seed = 1559937842;
    //seed = 1559942970;
    //seed = 1560196594;
    
    hashmap_t hmap;
    hashmap_t *map = &hmap;

    int i;
    int numtests = 10;
    for (i = 0; i < numtests; ++i)
    {
        runtest(map, 16000, seed);
    }

    printf("%s\n", "Passed performing random actions");

    return 0;
}

