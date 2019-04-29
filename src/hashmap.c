/**
 * @file hashmap.c
 * @author Craig Jacobson
 */
#include "hashmap.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <stdio.h>


/**
 * (2**32)/(Golden Ratio) ~= 2654435769
 * The two closest primes are 2654435761 and 2654435789
 */
static const uint32_t GAP = 2654435761;
static const int LEADING_BIT = 0x80000000;
static const int MASK = 0x7FFFFFFF;
static const int HASHMAP_MIN_LEN = 8;
#ifdef TEST_HASHMAP_NOSPACE
static const int HASHMAP_MAX_LEN = 1 << 4;
#else
static const int HASHMAP_MAX_LEN = 1 << 30;
#endif


#define HASHMAP_CLAMP(hash) (((hash) * GAP) >> (map->shift))
#define HASHMAP_SLOT(index) ((map->slots) + ((size_t)(index) * (size_t)(map->slotsize)))
#define HASHMAP_FULL(slot) (*((int *)(slot)) & LEADING_BIT)
#define HASHMAP_EMPTY(slot) (!(*((int *)(slot)) & LEADING_BIT))
#define HASHMAP_CLEAR(slot) (*((int *)slot) = 0)
#define HASHMAP_DEBT(slot) (*((int *)(slot)) & MASK)

static inline int
hashmap_pop_count(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

#define HASHMAP_BITS(n) (hashmap_pop_count(((uint32_t)(n) - 1)))
#define HASHMAP_MASK(n) ((n) - 1)
#define HASHMAP_SHIFT(n) (((sizeof(uint32_t)) * 8) - (n))
#define HASHMAP_NEXT(index) (((index) + 1) & map->mask)
#define HASHMAP_KEY(slot) ((slot) + sizeof(int))
#define HASHMAP_EL(slot) ((slot) + sizeof(int) + (map->keysize))

static inline int
hashmap_get_power_2(int n)
{
    if (n < HASHMAP_MIN_LEN)
    {
        return HASHMAP_MIN_LEN;
    }
    else if (n >= HASHMAP_MAX_LEN)
    {
        return HASHMAP_MAX_LEN;
    }
    else
    {
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        return (n + 1) >> 1;
    }
}


hashcode_t
hashmap_init(hashmap_t *map,
             int nslots,
             int keysize,
             int elsize,
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb)
{
    map->size = 0;
    map->len = hashmap_get_power_2(nslots);
    map->power = 2 * HASHMAP_BITS(map->len);
    map->shift = HASHMAP_SHIFT(HASHMAP_BITS(map->len));
    map->mask = HASHMAP_MASK(map->len);
    map->keysize = keysize;
    map->elsize = elsize;
    map->slotsize = sizeof(int) + keysize + elsize;
    map->hash_cb = hash_cb;
    map->eq_cb = eq_cb;
    map->slottmp = malloc(keysize + elsize);
    map->slotswap = malloc(keysize + elsize);
    const size_t alloclen = map->len * (sizeof(int) + keysize + elsize);
    map->slots = calloc(alloclen, 1);

    if (NULL == map->slottmp || NULL == map->slotswap || NULL == map->slots)
    {
        hashmap_destroy(map);

        return HASHCODE_NOMEM;
    }

    return HASHCODE_OK;
}

void
hashmap_destroy(hashmap_t *map)
{
    if (NULL != map->slottmp)
    {
        free(map->slottmp);
    }

    if (NULL != map->slotswap)
    {
        free(map->slotswap);
    }

    if (NULL != map->slots)
    {
        free(map->slots);
    }

    memset(map, 0, sizeof(hashmap_t));
}

int
hashmap_size(hashmap_t *map)
{
    return map->size;
}

int
hashmap_capacity(hashmap_t *map)
{
    return map->len;
}

bool
hashmap_is_empty(hashmap_t *map)
{
    return (0 == map->size);
}

void *
hashmap_get(hashmap_t *map,
            const void *_key)
{
    // Algorithm:
    //   Set debt to zero.
    //   Create hash.
    //   Create index by modulo prime size, incrementing as we advance.
    //   Lookup initial slot, incrementing as we advance.
    //   Track debt as we go, incrementing each time we advance.
    //   Test slot for fullness, hash, and equal key.
    //   If not found proceed until empty slot or debt is less than ours.
    const char *key = _key;

    int debt = 0;
    const uint32_t hash = map->hash_cb(key);
    int index = HASHMAP_CLAMP(hash);
    char *slot = HASHMAP_SLOT(index);
    do
    {
        if (HASHMAP_EMPTY(slot))
        {
            // Empty slot, no match.
            return NULL;
        }
        else if (map->eq_cb(HASHMAP_KEY(slot), key))
        {
            // Found it.
            return HASHMAP_EL(slot);
        }
        else if (HASHMAP_DEBT(slot) < debt)
        {
            // Entered rich neighborhood, no match.
            return NULL;
        }

        // Increment counters.
        ++debt;
        index = HASHMAP_NEXT(index);
        slot = HASHMAP_SLOT(index);
    }
    while (debt < map->power);

    // Checked as many as allowed before reallocation, no match.
    return NULL;
}

bool
hashmap_contains(hashmap_t *map,
                 const void *key)
{
    return !(NULL == hashmap_get(map, key));
}

hashcode_t
hashmap_iterate(hashmap_t *map,
                void *ud,
                hashmap_iterate_cb_t iter_cb)
{
    // Algorithm:
    //   For each full slot, call the callback with context and element.
    int index;
    for (index = 0; index < map->len; ++index)
    {
        char *slot = HASHMAP_SLOT(index);

        if (HASHMAP_FULL(slot))
        {
            char *key = HASHMAP_KEY(slot);
            char *el = key + map->keysize;
            if (iter_cb(ud, key, el))
            {
                return HASHCODE_STOP;
            }
        }
    }

    return HASHCODE_OK;
}

void
hashmap_clear(hashmap_t *map)
{
    map->size = 0;
    size_t len = ((size_t)map->len) * ((size_t)map->slotsize);
    memset(map->slots, 0, len);
}

void
hashmap_print(hashmap_t *map)
{
    printf("----------------------------------");
    // Algorithm:
    //   For each full slot, call the callback with context and element.
    int index;
    for (index = 0; index < map->len; ++index)
    {
        char *slot = HASHMAP_SLOT(index);
        printf("\n%d: %s", index, HASHMAP_FULL(slot) ? "full" : "empty");

        if (HASHMAP_FULL(slot))
        {
            char *key = HASHMAP_KEY(slot);
            printf(" = %d %d", *(int *)key, HASHMAP_DEBT(slot));
        }
    }
    printf("\n----------------------------------\n");
}

hashcode_t
hashmap_insert(hashmap_t *map,
               const void *_key,
               const void *_el,
               bool upsert)
{
    // Algorithm:
    //   Similar to hashmap_get.
    //   If we hit the max hits length we reallocate.
    //   If reallocation isn't possible, we return an error.
    const char *key = _key;
    const char *el = _el;

    int debt = 0;
    uint32_t hash = map->hash_cb(key);
    int index = HASHMAP_CLAMP(hash);
    //printf("Start index; %d for %d\n", index, *(int *)key);
    char *slot = HASHMAP_SLOT(index);
    do
    {
        if (HASHMAP_EMPTY(slot))
        {
            // Easy insert and done.
            *((int *)slot) = debt | LEADING_BIT;
            char *keyloc = HASHMAP_KEY(slot);
            char *elloc = keyloc + map->keysize;
            memcpy(keyloc, key, map->keysize);
            memcpy(elloc, el, map->elsize);
            ++map->size;
            //printf("OK.\n");
            return HASHCODE_OK;
        }
        else if (HASHMAP_DEBT(slot) < debt)
        {
            //printf("Lengths check: %d == %d\n", HASHMAP_MAX_LEN, map->size);
            if (HASHMAP_MAX_LEN == map->size)
            {
                // The check is done here because we want to allow upserts
                // but we don't want to incur the cost of checking up-front.
                //printf("NO SPACE.\n");
                return HASHCODE_NOSPACE;
            }

            // Steal from the rich.
            // Save the rich.
            memcpy(map->slotswap, slot + sizeof(int), map->slotsize);
            int debttmp = HASHMAP_DEBT(slot);

            // Insert the hash, key, element.
            *((int *)slot) = debt | LEADING_BIT;
            char *keyloc = HASHMAP_KEY(slot);
            char *elloc = HASHMAP_EL(slot);
            memcpy(keyloc, key, map->keysize);
            memcpy(elloc, el, map->elsize);

            // Swap the slot poiners.
            void *tmp = map->slotswap;
            map->slotswap = map->slottmp;
            map->slottmp = tmp;

            // Set current variables to point to rich variables.
            key = map->slottmp;
            el = map->slottmp + map->keysize;
            debt = debttmp;
            //printf("Swapped: %d\n", *(int *)key);

            upsert = false;
        }
        else if (map->eq_cb(HASHMAP_KEY(slot), key))
        {
            //printf("Exist value: %d\n", *(int *)HASHMAP_KEY(slot));
            //printf("Exist conflict: %d\n", *(int *)key);
            if (upsert)
            {
                void *elloc = HASHMAP_EL(slot);
                memcpy(elloc, el, map->elsize);
            }

            //printf("EXISTS.\n");
            return HASHCODE_EXIST;
        }
        else {
            // Continue search.
        }

        ++debt;
        index = HASHMAP_NEXT(index);
        slot = HASHMAP_SLOT(index);
    } while (debt < map->power);

    // We apparently need to reallocate, if possible.
    if (map->len < HASHMAP_MAX_LEN)
    {
        // Algorithm:
        //   Create a new hashmap one size larger.
        //   Add each item from this hashmap.
        //   Delete this hashmap.
        //   Copy new hashmap into place.
        //   Return result of this method recursively applied.
        //   Max calls is length of PRIMES - 1.

        hashmap_t newmap;
        hashcode_t code = hashmap_init(&newmap, map->len * 2,
                                       map->keysize, map->elsize,
                                       map->hash_cb, map->eq_cb);

        if (HASHCODE_OK != code)
        {
            return code;
        }

        index = 0;
        for (index = 0; index < map->len; ++index)
        {
            char *slot = HASHMAP_SLOT(index);

            if (HASHMAP_FULL(slot))
            {
                //printf("Reinsert: %d\n", *(int *)HASHMAP_KEY(slot));
                char *keytmp = HASHMAP_KEY(slot);
                char *eltmp = keytmp + map->keysize;
                code = hashmap_insert(&newmap, keytmp, eltmp, false);

                if (HASHCODE_OK != code)
                {
                    hashmap_destroy(&newmap);
                    return code;
                }
            }
        }
        //printf("Done expanding size.\n");


        code = hashmap_insert(&newmap, key, el, upsert);

        // We need to free after inserts because map->slottmp might be in use.
        hashmap_destroy(map);
        memcpy(map, &newmap, sizeof(hashmap_t));

        return code;
    }

    // We're at the end of our rope here.
    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_remove(hashmap_t *map,
               const void *_key,
               void *keyout,
               void *elout)
{
    // Algorithm:
    //   Similar to hashmap_get.
    //   If we don't find, then we're done.
    //   If found, copy to save if not null.
    //   If found, commence robin hooding.
    const char *key = _key;

    int debt = 0;
    const uint32_t hash = map->hash_cb(key);
    int index = HASHMAP_CLAMP(hash);
    char *slot = HASHMAP_SLOT(index);
    while (debt < map->power)
    {
        if (HASHMAP_EMPTY(slot))
        {
            return HASHCODE_NOEXIST;
        }
        else if (map->eq_cb(HASHMAP_KEY(slot), key))
        {
            --map->size;

            if (NULL != keyout)
            {
                void *keyloc = HASHMAP_KEY(slot);
                memcpy(keyout, keyloc, map->keysize);
            }

            if (NULL != elout)
            {
                void *elloc = HASHMAP_EL(slot);
                memcpy(elout, elloc, map->elsize);
            }

            HASHMAP_CLEAR(slot);
            ++debt;

            bool done = false;

            do
            {
                void *slotremoved = slot;
                void *slotsave = slot;
                int indexsave = index;

                while (debt < map->power)
                {
                    slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
                    index = (index + 1) < map->len ? (index + 1) : 0;

                    if (HASHMAP_EMPTY(slot))
                    {
                        done = true;
                        // Encountered blank space.
                        break;
                    }
                    else if (HASHMAP_DEBT(slot) < debt)
                    {
                        // We encountered a new run.
                        break;
                    }

                    ++debt;
                    slotsave = slot;
                    indexsave = index;
                }

                if (debt == map->power)
                {
                    slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
                    index = (index + 1) < map->len ? (index + 1) : 0;
                }

                if (slotsave != slotremoved)
                {
                    // Copy slot at end of run to removed location.
                    memcpy(slotremoved, slotsave, map->slotsize);
                    HASHMAP_CLEAR(slotsave);
                }

                if (done)
                {
                    break;
                }

                debt = HASHMAP_DEBT(slot);
                slot = slotsave;
                index = indexsave;
            }
            while (0 != debt);

            return HASHCODE_OK;
        }
        else if (HASHMAP_DEBT(slot) < debt)
        {
            return HASHCODE_NOEXIST;
        }

        ++debt;
        slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
        index = (index + 1) < map->len ? (index + 1) : 0;
    }

    return HASHCODE_NOEXIST;
}


