
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"


typedef enum hashstate_e
{
    HASHSTATE_OUT = 0,
    HASHSTATE_IN  = 1,
} hashstate_t;

typedef enum hashaction_e
{
    HASHACTION_CON = 0, // Contains.
    HASHACTION_INS = 1, // Insert.
    HASHACTION_DEL = 2, // Delete/Remove.
} hashaction_t;

#define MAX_ACTIONS (3)

typedef struct hashel_s
{
    int val;
    hashstate_t state;
    hashaction_t actions[MAX_ACTIONS];
} hashel_t;

static uint32_t
hash_cb(const void *key)
{
    return (uint32_t)(*((int *)key));
}

static bool
eq_cb(const void *key1, const void *key2)
{
    int k1 = *((int *)key1);
    int k2 = *((int *)key2);
    return k1 == k2;
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
    // const int SEED = 1552696998;

    printf("SEED: %d\n", SEED);

    srand(SEED);
    
    hashmap_t map;

    hashmap_init(&map, 0, sizeof(int), 0, hash_cb, eq_cb);

    int len = myrand(1, 20000);
    hashel_t *els = calloc(len, sizeof(hashel_t));
    int step = myrand(1, 2000);
    int lowval = myrand(-300, 0);
    int nextval = lowval;

    int i;
    for (i = 0; i < len; ++i)
    {
        hashel_t *el = &els[i];
        el->val = nextval;
        nextval = nextval + myrand(1, step);
        el->state = HASHSTATE_OUT;
        int j;
        for (j = 0; j < MAX_ACTIONS; ++j)
        {
            el->actions[j] = myrand(HASHACTION_CON, HASHACTION_DEL);
        }
    }

    printf("%s\n", "Passed generating random elements to insert");

    int size = 0;
    int k;
    for (k = 0; k < MAX_ACTIONS; ++k)
    {
        for (i = 0; i < len; ++i)
        {
            hashel_t *el = &els[i];

            switch (el->actions[k])
            {
                case HASHACTION_CON:
                {
                    if (HASHSTATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(&map, &el->val) && "Failed contains test");
                    }
                    else
                    {
                        assert(hashmap_contains(&map, &el->val) && "Failed contains test");
                    }
                    assert(size == hashmap_size(&map) && "Failed size test");
                }
                break;
                case HASHACTION_INS:
                {
                    if (HASHSTATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(&map, &el->val) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_insert(&map, &el->val, NULL, false) && "Failed insert not exist");
                        ++size;
                    }
                    else
                    {
                        assert(hashmap_contains(&map, &el->val) && "Failed contains test");
                        assert(HASHCODE_EXIST == hashmap_insert(&map, &el->val, NULL, false) && "Failed insert exists test");
                    }
                    el->state = HASHSTATE_IN;
                    assert(size == hashmap_size(&map) && "Failed size test");
                    assert(hashmap_contains(&map, el) && "Failed contains test");
                }
                break;
                case HASHACTION_DEL:
                {
                    int out = -1;
                    if (HASHSTATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(&map, &el->val) && "Failed contains test");
                        assert(HASHCODE_NOEXIST == hashmap_remove(&map, &el->val, &out, NULL) && "Failed remove not exist");
                        assert(-1 == out && "Failed no modify if not in");
                    }
                    else
                    {
                        assert(hashmap_contains(&map, &el->val) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_remove(&map, &el->val, &out, NULL) && "Failed remove test");
                        assert(el->val == out && "Failed successful remove");
                        --size;
                    }
                    el->state = HASHSTATE_OUT;
                    assert(size == hashmap_size(&map) && "Failed size test");
                    assert(!hashmap_contains(&map, el) && "Failed contains test");
                }
                break;
            }
        }
    }

    printf("%s\n", "Passed performing random actions");

    hashmap_destroy(&map);

    return 0;
}

