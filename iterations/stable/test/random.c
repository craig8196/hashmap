
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
    
    hashmap_t map;

    hashmap_init(&map, 0, hash_cb, eq_cb);

    int len = myrand(1, 250);
    hashel_t *els = calloc(len, sizeof(hashel_t));
    int step = myrand(1, 5);
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
                        assert(!hashmap_contains(&map, el) && "Failed contains test");
                    }
                    else
                    {
                        assert(hashmap_contains(&map, el) && "Failed contains test");
                    }
                    assert(size == hashmap_size(&map) && "Failed size test");
                }
                break;
                case HASHACTION_INS:
                {
                    void *out = NULL;
                    if (HASHSTATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(&map, el) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_insert(&map, el, &out) && "Failed insert not exist");
                        assert(NULL == out && "Failed no modify if in");
                        ++size;
                    }
                    else
                    {
                        assert(hashmap_contains(&map, el) && "Failed contains test");
                        assert(HASHCODE_EXIST == hashmap_insert(&map, el, &out) && "Failed insert exists test");
                        assert((void *)el == out && "Failed successful upsert");
                    }
                    el->state = HASHSTATE_IN;
                    assert(size == hashmap_size(&map) && "Failed size test");
                    assert(hashmap_contains(&map, el) && "Failed contains test");
                }
                break;
                case HASHACTION_DEL:
                {
                    void *out = NULL;
                    if (HASHSTATE_OUT == el->state)
                    {
                        assert(!hashmap_contains(&map, el) && "Failed contains test");
                        assert(HASHCODE_NOEXIST == hashmap_remove(&map, el, &out) && "Failed remove not exist");
                        assert(NULL == out && "Failed no modify if not in");
                    }
                    else
                    {
                        assert(hashmap_contains(&map, el) && "Failed contains test");
                        assert(HASHCODE_OK == hashmap_remove(&map, el, &out) && "Failed remove test");
                        assert((void *)el == out && "Failed successful remove");
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

