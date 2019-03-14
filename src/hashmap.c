/**
 * @file hashmap.c
 * @author Craig Jacobson
 */
#include "hashmap.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


static const uint32_t LEADING_BIT = 0x80000000;
static const uint32_t MASK = 0x7FFFFFFF;
static const int PRIMES[] =
{
    1,
    3,
    5,
    7,
    11,
#ifndef TEST_HASHMAP_NOSPACE
    17,
    23,
    29,
    41,
    59,
    83,
    113,
    157,
    223,
    307,
    431,
    599,
    839,
    1181,
    1657,
    2297,
    3217,
    4507,
    6301,
    8821,
    12373,
    17291,
    24203,
    33889,
    47441,
    66413,
    92987,
    130171,
    182233,
    255121,
    357169,
    500029,
    700057,
    980069,
    1387649,
    1923127,
    2691401,
    3766703,
    5276969,
    7385767,
    10332557,
    14484739,
    20253691,
    28352173,
    39688177,
    55572581,
    77792227,
    108908411,
    152471503,
    213461819,
    298836649,
    418363973,
#endif
};
static const int PRIMES_LAST = (sizeof(PRIMES)/sizeof(PRIMES[0])) - 1;

static int
hashmap_internal_prime_modulo(int prime_index, uint32_t index)
{
    switch (prime_index)
    {
        case 0: index = 0; break;
        case 1: index = index % (uint32_t)3; break;
        case 2: index = index % (uint32_t)5; break;
        case 3: index = index % (uint32_t)7; break;
        case 4: index = index % (uint32_t)11; break;
#ifndef TEST_HASHMAP_NOSPACE
        case 5: index = index % (uint32_t)17; break;
        case 6: index = index % (uint32_t)23; break;
        case 7: index = index % (uint32_t)29; break;
        case 8: index = index % (uint32_t)41; break;
        case 9: index = index % (uint32_t)59; break;
        case 10: index = index % (uint32_t)83; break;
        case 11: index = index % (uint32_t)113; break;
        case 12: index = index % (uint32_t)157; break;
        case 13: index = index % (uint32_t)223; break;
        case 14: index = index % (uint32_t)307; break;
        case 15: index = index % (uint32_t)431; break;
        case 16: index = index % (uint32_t)599; break;
        case 17: index = index % (uint32_t)839; break;
        case 18: index = index % (uint32_t)1181; break;
        case 19: index = index % (uint32_t)1657; break;
        case 20: index = index % (uint32_t)2297; break;
        case 21: index = index % (uint32_t)3217; break;
        case 22: index = index % (uint32_t)4507; break;
        case 23: index = index % (uint32_t)6301; break;
        case 24: index = index % (uint32_t)8821; break;
        case 25: index = index % (uint32_t)12373; break;
        case 26: index = index % (uint32_t)17291; break;
        case 27: index = index % (uint32_t)24203; break;
        case 28: index = index % (uint32_t)33889; break;
        case 29: index = index % (uint32_t)47441; break;
        case 30: index = index % (uint32_t)66413; break;
        case 31: index = index % (uint32_t)92987; break;
        case 32: index = index % (uint32_t)130171; break;
        case 33: index = index % (uint32_t)182233; break;
        case 34: index = index % (uint32_t)255121; break;
        case 35: index = index % (uint32_t)357169; break;
        case 36: index = index % (uint32_t)500029; break;
        case 37: index = index % (uint32_t)700057; break;
        case 38: index = index % (uint32_t)980069; break;
        case 39: index = index % (uint32_t)1387649; break;
        case 40: index = index % (uint32_t)1923127; break;
        case 41: index = index % (uint32_t)2691401; break;
        case 42: index = index % (uint32_t)3766703; break;
        case 43: index = index % (uint32_t)5276969; break;
        case 44: index = index % (uint32_t)7385767; break;
        case 45: index = index % (uint32_t)10332557; break;
        case 46: index = index % (uint32_t)14484739; break;
        case 47: index = index % (uint32_t)20253691; break;
        case 48: index = index % (uint32_t)28352173; break;
        case 49: index = index % (uint32_t)39688177; break;
        case 50: index = index % (uint32_t)55572581; break;
        case 51: index = index % (uint32_t)77792227; break;
        case 52: index = index % (uint32_t)108908411; break;
        case 53: index = index % (uint32_t)152471503; break;
        case 54: index = index % (uint32_t)213461819; break;
        case 55: index = index % (uint32_t)298836649; break;
        case 56: index = index % (uint32_t)418363973; break;
#endif
    }

    return (int)index;
}

static inline char *
hashmap_internal_get_slot(hashmap_t *map, int index)
{
    return (map->slots) + ((size_t)index * (size_t)map->slotsize);
}

static inline bool
hashmap_internal_slot_full(void *slot)
{
    return *((uint32_t *)slot) & LEADING_BIT ? true : false;
}

static inline bool
hashmap_internal_slot_empty(void *slot)
{
    return *((uint32_t *)slot) & LEADING_BIT ? false : true;
}

static inline void
hashmap_internal_slot_clear(void *slot)
{
    *((uint32_t *)slot) = 0;
}

static inline bool
hashmap_internal_slot_hash_eq(char *slot, uint32_t hash)
{
    return (*((uint32_t *)slot) & MASK) == hash;
}

static inline bool
hashmap_internal_slot_debt_lt(hashmap_t *map, void *slot, int index, int debt)
{
    uint32_t hash = (*((uint32_t *)slot)) & MASK;
    int slotdebt = hashmap_internal_prime_modulo(map->pindex, hash) - index;

    if (slotdebt < 0)
    {
        slotdebt = map->len + slotdebt;
    }

    return (slotdebt < debt);
}

static inline int
hashmap_internal_hash_debt(hashmap_t *map, int index, uint32_t hash)
{
    int debt = hashmap_internal_prime_modulo(map->pindex, hash) - index;

    if (debt < 0)
    {
        debt = map->len + debt;
    }

    return debt;
}

static int
hashmap_internal_determine_prime_index(int nslots)
{
    if (nslots <= PRIMES[0])
    {
        return 0;
    }
    else if (nslots >= PRIMES[PRIMES_LAST])
    {
        return PRIMES_LAST;
    }
    else
    {
        int start = 1;
        int end = PRIMES_LAST - 1;
        while (start <= end)
        {
            int mid = (start + end)/2;
            if (nslots == PRIMES[mid])
            {
                return mid;
            }
            else if (nslots < PRIMES[mid])
            {
                end = mid - 1;
            }
            else {
                start = mid + 1;
            }
        }

        return start;
    }
}

static int
hashmap_internal_log2(unsigned int num)
{
    int count = 0;

    while (num)
    {
        num >>= 1;
        ++count;
    }

    return count;
}

static uint32_t
hashmap_internal_pointer_hash_cb(const void *el)
{
    if (8 == sizeof(void *))
    {
        uint32_t *element = (uint32_t *)(&el);
        return (element[0] ^ element[1]);
    }
    else if (4 == sizeof(void *))
    {
        uint32_t *element = (uint32_t *)(&el);
        return element[0];
    }
    else
    {
        int len = sizeof(void *);
        uint8_t *bytes = (uint8_t *)(&el);
        uint32_t result = bytes[0];
        int i;
        for (i = 1; i < len; ++i)
        {
            result *= (uint32_t)bytes[i];
        }
        return result;
    }
}

static bool
hashmap_internal_pointer_eq_cb(const void *el1, const void *el2)
{
    return (el1 == el2);
}

hashcode_t
hashmap_init(hashmap_t *map,
             int nslots,
             int keysize,
             int elsize,
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb)
{
    int pindex = hashmap_internal_determine_prime_index(nslots);

    memset(map, 0, sizeof(hashmap_t));

    // map->size = 0; // Already done.
    map->len = PRIMES[pindex];
    map->pindex = pindex;

    if (PRIMES_LAST == pindex)
    {
        map->maxrun = PRIMES[PRIMES_LAST];
    }
    else
    {
        map->maxrun = hashmap_internal_log2((unsigned int)PRIMES[map->pindex]);
    }

    map->keysize = keysize;
    map->elsize = elsize;
    map->slotsize = sizeof(uint32_t) + keysize + elsize;

    if (NULL != hash_cb)
    {
        map->hash_cb = hash_cb;
    }
    else 
    {
        map->hash_cb = hashmap_internal_pointer_hash_cb;
    }
    if (NULL != eq_cb)
    {
        map->eq_cb = eq_cb;
    }
    else
    {
        map->eq_cb = hashmap_internal_pointer_eq_cb;
    }

    map->slottmp = malloc(keysize + elsize);
    map->slotswap = malloc(keysize + elsize);
    size_t alloclen = map->len * (sizeof(uint32_t) + keysize + elsize);
    map->slots = calloc(alloclen, 1);

    if (NULL == map->slottmp || NULL == map->slotswap || NULL == map->slots)
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

        return HASHCODE_NOMEM;
    }

    return HASHCODE_OK;
}

void
hashmap_destroy(hashmap_t *map)
{
    if (map->slots)
    {
        free(map->slots);
        memset(map, 0, sizeof(hashmap_t));
    }
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
    //   Create hash.
    //   Create index by modulo prime size.
    //   Track debt as we go.
    //   Lookup slot.
    //   Test slot for fullness, hash, and equal key.
    //   If not found proceed until empty slot or debt is less than ours.
    const char *key = _key;

    int debt = 0;
    const uint32_t hash = map->hash_cb(key) & MASK;
    int index = hashmap_internal_prime_modulo(map->pindex, hash);
    char *slot = hashmap_internal_get_slot(map, index);
    do
    {

        if (hashmap_internal_slot_empty(slot))
        {
            // Empty slot, no match.
            return NULL;
        }
        else if (hashmap_internal_slot_hash_eq(slot, hash)
                 && map->eq_cb(slot + sizeof(uint32_t), key))
        {
            // Found it.
            return (slot + sizeof(uint32_t) + map->keysize);
        }
        else if (hashmap_internal_slot_debt_lt(map, slot, index, debt))
        {
            // Entered rich neighborhood, no match.
            return NULL;
        }

        ++debt;
        slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
        index = (index + 1) < map->len ? (index + 1) : 0;
    }
    while (debt < map->maxrun);

    // Checked as many as allowed before reallocation, no match.
    return NULL;
}

bool
hashmap_contains(hashmap_t *map,
                 const void *key)
{
    return NULL == hashmap_get(map, key) ? false : true;
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
        char *slot = hashmap_internal_get_slot(map, index);

        if (hashmap_internal_slot_full(slot))
        {
            char *key = slot + sizeof(uint32_t);
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
    uint32_t hash = map->hash_cb(key) & MASK;
    int index = hashmap_internal_prime_modulo(map->pindex, hash);
    char *slot = hashmap_internal_get_slot(map, index);
    while (debt < map->maxrun)
    {
        if (hashmap_internal_slot_empty(slot))
        {
            // Easy insert and done.
            *((uint32_t *)slot) = hash | LEADING_BIT;
            char *keyloc = slot + sizeof(uint32_t);
            char *elloc = keyloc + map->keysize;
            memcpy(keyloc, key, map->keysize);
            memcpy(elloc, el, map->elsize);
            ++map->size;
            return HASHCODE_OK;
        }
        else if (hashmap_internal_slot_debt_lt(map, slot, index, debt))
        {
            if (PRIMES_LAST == map->pindex) {
                // The check is done here because we want to allow upserts
                // but we don't want to incur the cost of checking up-front.
                return HASHCODE_NOSPACE;
            }

            // Steal from the rich.
            // Save the rich.
            memcpy(map->slotswap, slot + sizeof(uint32_t), map->slotsize);
            uint32_t hashtmp = *((uint32_t *)slot) & MASK;

            // Insert the hash, key, element.
            *((uint32_t *)slot) = hash | LEADING_BIT;
            char *keyloc = slot + sizeof(uint32_t);
            char *elloc = keyloc + map->keysize;
            memcpy(keyloc, key, map->keysize);
            memcpy(elloc, el, map->elsize);

            // Swap the slot poiners.
            void *tmp = map->slotswap;
            map->slotswap = map->slottmp;
            map->slottmp = tmp;

            // Set current variables to point to rich variables.
            key = map->slottmp;
            el = map->slottmp + map->keysize;
            hash = hashtmp;

            debt = hashmap_internal_hash_debt(map, index, hash);
            upsert = false;
        }
        else if (hashmap_internal_slot_hash_eq(slot, hash)
                 && map->eq_cb(slot + sizeof(uint32_t), key))
        {
            if (upsert)
            {
                void *elloc = slot + sizeof(uint32_t) + map->keysize;
                memcpy(elloc, el, map->elsize);
            }

            return HASHCODE_EXIST;
        }
        else {
            // Continue search.
        }

        ++debt;
        slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
        index = (index + 1) < map->len ? (index + 1) : 0;
    }

    if (map->pindex < PRIMES_LAST)
    {
        // Algorithm:
        //   Create a new hashmap one size larger.
        //   Add each item from this hashmap.
        //   Delete this hashmap.
        //   Copy new hashmap into place.
        //   Return result of this method recursively applied.
        //   Max calls is length of PRIMES - 1.

        int pindex = map->pindex + 1;
        hashmap_t newmap;
        hashcode_t code = hashmap_init(&newmap, PRIMES[pindex],
                                       map->keysize, map->elsize,
                                       map->hash_cb, map->eq_cb);

        if (HASHCODE_OK != code)
        {
            return code;
        }

        index = 0;
        for (index = 0; index < map->len; ++index)
        {
            char *slot = hashmap_internal_get_slot(map, index);

            if (hashmap_internal_slot_full(slot))
            {
                char *key = slot + sizeof(uint32_t);
                char *el = key + map->keysize;
                code = hashmap_insert(&newmap, key, el, false);

                if (HASHCODE_OK != code)
                {
                    hashmap_destroy(&newmap);
                    return code;
                }
            }
        }

        return HASHCODE_OK;
    }

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
    const uint32_t hash = map->hash_cb(key) & MASK;
    int index = hashmap_internal_prime_modulo(map->pindex, hash);
    char *slot = hashmap_internal_get_slot(map, index);
    while (debt < map->maxrun)
    {
        if (hashmap_internal_slot_empty(slot))
        {
            return HASHCODE_NOEXIST;
        }
        else if (hashmap_internal_slot_hash_eq(slot, hash)
                 && map->eq_cb(slot + sizeof(uint32_t), key))
        {
            if (keyout)
            {
                void *keyloc = slot + sizeof(uint32_t);
                memcpy(keyout, keyloc, map->keysize);
            }

            if (elout)
            {
                void *elloc = slot + sizeof(uint32_t) + map->keysize;
                memcpy(elout, elloc, map->elsize);
            }

            hashmap_internal_slot_clear(slot);

            void *slotremoved = slot;
            void *slotsave = slot;

            ++debt;
            while (debt < map->maxrun)
            {
                slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
                index = (index + 1) < map->len ? (index + 1) : 0;

                if (hashmap_internal_slot_debt_lt(map, slot, index, debt))
                {
                    // We encountered a new run.
                    break;
                }

                ++debt;
                slotsave = slot;
            }

            // Slot should point to the end of our run.
            if (slotsave != slotremoved)
            {
                memcpy(slotremoved, slotsave, map->slotsize);
                hashmap_internal_slot_clear(slotsave);
            }

            return HASHCODE_OK;
        }
        else if (hashmap_internal_slot_debt_lt(map, slot, index, debt))
        {
            return HASHCODE_NOEXIST;
        }

        ++debt;
        slot = (index + 1) < map->len ? slot + map->slotsize : map->slots;
        index = (index + 1) < map->len ? (index + 1) : 0;
    }

    return HASHCODE_NOEXIST;
}


