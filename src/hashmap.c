/**
 * @file hashmap.c
 * @author Craig Jacobson
 */
#include "hashmap.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// TODO remove
#include <stdio.h>


// TODO perform more reliable insert speed test
//TODO the increase in size is inefficient because the hash is called again!!
// reduce the cost by iterating manually!!

static const int LEADING_BIT = 1 << ((sizeof(int) * 8) - 1);
static const int CLEAR_LEADING_BIT = ~(1 << ((sizeof(int) * 8) - 1));
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
static const int PRIMES_LEN = sizeof(PRIMES)/sizeof(PRIMES[0]);
static const int PRIMES_LAST = (sizeof(PRIMES)/sizeof(PRIMES[0])) - 1;

static int
hashmap_internal_prime_modulo(int prime_index, uint32_t index)
{
    switch (prime_index)
    {
        case 0: index = 0; break;
        case 1: index = index % 3; break;
        case 2: index = index % 5; break;
        case 3: index = index % 7; break;
        case 4: index = index % 11; break;
#ifndef TEST_HASHMAP_NOSPACE
        case 5: index = index % 17; break;
        case 6: index = index % 23; break;
        case 7: index = index % 29; break;
        case 8: index = index % 41; break;
        case 9: index = index % 59; break;
        case 10: index = index % 83; break;
        case 11: index = index % 113; break;
        case 12: index = index % 157; break;
        case 13: index = index % 223; break;
        case 14: index = index % 307; break;
        case 15: index = index % 431; break;
        case 16: index = index % 599; break;
        case 17: index = index % 839; break;
        case 18: index = index % 1181; break;
        case 19: index = index % 1657; break;
        case 20: index = index % 2297; break;
        case 21: index = index % 3217; break;
        case 22: index = index % 4507; break;
        case 23: index = index % 6301; break;
        case 24: index = index % 8821; break;
        case 25: index = index % 12373; break;
        case 26: index = index % 17291; break;
        case 27: index = index % 24203; break;
        case 28: index = index % 33889; break;
        case 29: index = index % 47441; break;
        case 30: index = index % 66413; break;
        case 31: index = index % 92987; break;
        case 32: index = index % 130171; break;
        case 33: index = index % 182233; break;
        case 34: index = index % 255121; break;
        case 35: index = index % 357169; break;
        case 36: index = index % 500029; break;
        case 37: index = index % 700057; break;
        case 38: index = index % 980069; break;
        case 39: index = index % 1387649; break;
        case 40: index = index % 1923127; break;
        case 41: index = index % 2691401; break;
        case 42: index = index % 3766703; break;
        case 43: index = index % 5276969; break;
        case 44: index = index % 7385767; break;
        case 45: index = index % 10332557; break;
        case 46: index = index % 14484739; break;
        case 47: index = index % 20253691; break;
        case 48: index = index % 28352173; break;
        case 49: index = index % 39688177; break;
        case 50: index = index % 55572581; break;
        case 51: index = index % 77792227; break;
        case 52: index = index % 108908411; break;
        case 53: index = index % 152471503; break;
        case 54: index = index % 213461819; break;
        case 55: index = index % 298836649; break;
        case 56: index = index % 418363973; break;
#endif
    }

    return (int)index;
}

static int
hashmap_internal_determine_prime_index(int nslots)
{
    int nbuckets = nslots/HASHMAP_BUCKET_COUNT;

    if (nbuckets <= PRIMES[0])
    {
        return 0;
    }
    else if (nbuckets >= PRIMES[PRIMES_LAST])
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
            //printf("%d, %d, %d, %d\n", start, end, mid, PRIMES[mid]);
            if (nbuckets == PRIMES[mid])
            {
                return mid;
            }
            else if (nbuckets < PRIMES[mid])
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

static hashcode_t
hashmap_internal_init(hashmap_t *map,
                      int nslots_index,
                      hashmap_hash_cb_t hash_cb,
                      hashmap_eq_cb_t eq_cb)
{
    memset(map, 0, sizeof(hashmap_t));
    map->nslots_index = nslots_index;
    if (PRIMES_LAST == map->nslots_index)
    {
        map->max_hits = PRIMES[PRIMES_LAST];
    }
    else
    {
        map->max_hits = hashmap_internal_log2((unsigned int)PRIMES[map->nslots_index]) * 2;
    }
    map->hash_cb = hash_cb;
    map->eq_cb = eq_cb;

    // NOTE: This aligns us to to x86_64 cache line.
    void *buckets;
    size_t alloclen = sizeof(hashmap_bucket_t)
                      * (size_t)PRIMES[map->nslots_index];
    int error = posix_memalign(&buckets, 64, alloclen);
                               
    if (0 != error)
    {
        memset(map, 0, sizeof(hashmap_t));
        return HASHCODE_NOMEM;
    }
    else
    {
        map->buckets = (hashmap_bucket_t *)buckets;
        memset(buckets, 0, alloclen);
    }

    return HASHCODE_OK;
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
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb)
{
    int nslots_index = hashmap_internal_determine_prime_index(nslots);

    if (NULL == hash_cb)
    {
        hash_cb = hashmap_internal_pointer_hash_cb;
    }
    if (NULL == eq_cb)
    {
        eq_cb = hashmap_internal_pointer_eq_cb;
    }

    return hashmap_internal_init(map, nslots_index, hash_cb, eq_cb);
}

void
hashmap_destroy(hashmap_t *map)
{
    if (map->buckets)
    {
        free(map->buckets);
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
    return PRIMES[map->nslots_index] * HASHMAP_BUCKET_COUNT;
}

bool
hashmap_is_empty(hashmap_t *map)
{
    return (0 == map->size);
}

hashcode_t
hashmap_get(hashmap_t *map,
            void *el,
            void **save)
{
    // Algorithm:
    //   Create hash.
    //   Create index by modulo prime size.
    //   Lookup bucket and check the entries, if any, for hash.
    //   If bucket is full and not found, proceed to next bucket by offset of 1.
    int debt;
    int index;
    int len = map->max_hits;
    const uint32_t hash = map->hash_cb(el);
    for (debt = 0,
         index = hashmap_internal_prime_modulo(map->nslots_index, hash);
         debt < len;
         ++debt,
         index = hashmap_internal_prime_modulo(map->nslots_index, (uint32_t)(index + 1)))
    {
        hashmap_bucket_t *bucket = &map->buckets[index];
        int i;
        for (i = 0; i < HASHMAP_BUCKET_COUNT; ++i)
        {
            if (!(LEADING_BIT & bucket->slots[i].debt))
            {
                return HASHCODE_NOEXIST;
            }

            if (bucket->slots[i].hash == hash
                && map->eq_cb(el, bucket->slots[i].el))
            {
                (*save) = bucket->slots[i].el;
                return HASHCODE_OK;
            }
            else if ((bucket->slots[i].debt & CLEAR_LEADING_BIT) < debt)
            {
                // We've encountered someone richer; short circuit.
                // According to the Robin Hood algorithm, we are 
                // further from home and thus, we would steal from the rich
                // on insertion of this value.
                // Basically, we're out of the poor neighborhood, which is
                // ours.
                return HASHCODE_NOEXIST;
            }
        }
    }

    return HASHCODE_NOEXIST;
}

bool
hashmap_contains(hashmap_t *map,
                 void *el)
{
    void *save = NULL;
    return HASHCODE_OK == hashmap_get(map, el, &save) ? true : false;
}

hashcode_t
hashmap_iterate(hashmap_t *map,
                void *ud,
                hashmap_iterate_cb_t iter_cb)
{
    // Algorithm:
    //   For each full slot, call the callback with context and element.
    int index;
    int len = PRIMES[map->nslots_index];
    for (index = 0; index < len; ++index)
    {
        hashmap_bucket_t *bucket = &map->buckets[index];
        int i;
        for (i = 0; i < HASHMAP_BUCKET_COUNT; ++i)
        {
            if ((LEADING_BIT & bucket->slots[i].debt))
            {
                hashcode_t code = iter_cb(ud, bucket->slots[i].el);
                if (HASHCODE_OK != code)
                {
                    return code;
                }
            }
        }
    }

    return HASHCODE_OK;
}

void
hashmap_clear(hashmap_t *map)
{
    map->size = 0;

    memset(map->buckets,
           0,
           (size_t)PRIMES[map->nslots_index] * sizeof(hashmap_bucket_t));
}

static hashcode_t
hashmap_internal_readd_cb(void *ud, void *el)
{
    hashmap_t *map = (hashmap_t *)ud;
    return hashmap_insert(map, el, NULL);
}

hashcode_t
hashmap_insert(hashmap_t *map,
               void *el,
               void **upsert)
{
    // Algorithm:
    //   Similar to hashmap_get.
    //   If we hit the max hits length we reallocate.
    //   If reallocation isn't possible, we return an error.
    int debt;
    int index;
    int len = map->max_hits;
    uint32_t hash = map->hash_cb(el);
    for (debt = 0,
         index = hashmap_internal_prime_modulo(map->nslots_index, hash);
         debt < len;
         ++debt,
         index = hashmap_internal_prime_modulo(map->nslots_index, (uint32_t)(index + 1)))
    {
        hashmap_bucket_t *bucket = &map->buckets[index];
        int i;
        for (i = 0; i < HASHMAP_BUCKET_COUNT; ++i)
        {
            if (!(LEADING_BIT & bucket->slots[i].debt))
            {
                bucket->slots[i].debt = LEADING_BIT | debt;
                bucket->slots[i].hash = hash;
                bucket->slots[i].el = el;
                ++map->size;

                return HASHCODE_OK;
            }
            else if (bucket->slots[i].hash == hash
                     && map->eq_cb(el, bucket->slots[i].el))
            {
                if (NULL != upsert)
                {
                    (*upsert) = bucket->slots[i].el;
                    bucket->slots[i].el = el;
                }

                return HASHCODE_EXIST;
            }
            else if ((CLEAR_LEADING_BIT & bucket->slots[i].debt) < debt)
            {
                // Steal from the rich.
                int tmpdebt = CLEAR_LEADING_BIT & bucket->slots[i].debt;
                uint32_t tmphash = bucket->slots[i].hash;
                void *tmpel = bucket->slots[i].el;
                bucket->slots[i].debt = debt | LEADING_BIT;
                bucket->slots[i].hash = hash;
                bucket->slots[i].el = el;
                debt = tmpdebt;
                hash = tmphash;
                el = tmpel;
                upsert = NULL;
            }
            else
            {
                // Keep searching for a home.
            }
        }
    }

    if (map->nslots_index < (PRIMES_LEN - 1))
    {
        // Algorithm:
        //   Create a new hashmap one size larger.
        //   Add each item from this hashmap.
        //   Delete this hashmap.
        //   Copy new hashmap into place.
        //   Return result of this method recursively applied.
        //   Max calls is length of PRIMES - 1.
        hashmap_t newmap;
        int error = hashmap_internal_init(&newmap,
                                          map->nslots_index + 1,
                                          map->hash_cb, map->eq_cb);
        if (error)
        {
            return error;
        }
        hashcode_t code = hashmap_iterate(map,
                                          &newmap, hashmap_internal_readd_cb);
        if (HASHCODE_OK != code)
        {
            return code;
        }
        hashmap_destroy(map);
        memcpy(map, &newmap, sizeof(hashmap_t));
        return hashmap_insert(map, el, upsert);
    }

    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_remove(hashmap_t *map,
               void *el,
               void **save)
{
    // Algorithm:
    //   Similar to hashmap_get.
    //   If we don't find, then we're done.
    //   If found, copy to save if not null.
    //   If found, commence robin hooding.
    int debt;
    int index;
    int len = map->max_hits;
    uint32_t hash = map->hash_cb(el);
    for (debt = 0,
         index = hashmap_internal_prime_modulo(map->nslots_index, hash);
         debt < len;
         ++debt,
         index = hashmap_internal_prime_modulo(map->nslots_index, (uint32_t)(index + 1)))
    {
        hashmap_bucket_t *bucket = &map->buckets[index];
        int i;
        for (i = 0; i < HASHMAP_BUCKET_COUNT; ++i)
        {
            if (!(LEADING_BIT & bucket->slots[i].debt))
            {
                //printf("Doesn't contain value: %d\n", (int)((intptr_t)el));
                return HASHCODE_NOEXIST;
            }
            else if (bucket->slots[i].hash == hash
                     && map->eq_cb(el, bucket->slots[i].el))
            {
                //printf("Contains value: %d\n", (int)((intptr_t)el));
                if (NULL != save)
                {
                    (*save) = bucket->slots[i].el;
                }

                --map->size;
                bucket->slots[i].debt = 0;

                // Backshift propagation.
                hashmap_slot_t *dead = NULL;

                for ( ; ; )
                {
                    // Full bucket test, if the end bucket is full we continue.
                    // Otherwise, we do backshift and quit.
                    if (LEADING_BIT & bucket->slots[HASHMAP_BUCKET_LAST].debt
                        || HASHMAP_BUCKET_LAST == i)
                    {
                        dead = &bucket->slots[HASHMAP_BUCKET_LAST];
                    }
                    else
                    {
                        switch (i)
                        {
                            case 0:
                                memmove(&bucket->slots[0],
                                        &bucket->slots[1],
                                        sizeof(hashmap_slot_t) * 2);
                            break;
                            case 1:
                                memmove(&bucket->slots[1],
                                        &bucket->slots[2],
                                        sizeof(hashmap_slot_t));
                            break;
                            default:
                            break;
                        }

                        bucket->slots[HASHMAP_BUCKET_LAST - 1].debt = 0;

                        break;
                    }

                    // Backshift locally if we're not at the end of the bucket.
                    if ((HASHMAP_BUCKET_LAST) != i)
                    {
                        memmove(&bucket->slots[i],
                                &bucket->slots[i + 1],
                                sizeof(hashmap_slot_t)
                                * (HASHMAP_BUCKET_LAST - i));
                        // Prevent case where we don't replace the dead slot
                        // with a value further on.
                        dead->debt = 0;
                    }
                    i = 0;

                    index = hashmap_internal_prime_modulo(map->nslots_index, (uint32_t)(index + 1));
                    bucket = &map->buckets[index];

                    if (!(bucket->slots[0].debt & LEADING_BIT))
                    {
                        break;
                    }

                    if (0 != (bucket->slots[0].debt & CLEAR_LEADING_BIT))
                    {
                        --bucket->slots[0].debt;
                        (*dead) = bucket->slots[0];
                        dead = NULL;
                    }
                    else
                    {
                        break;
                    }
                }

                return HASHCODE_OK;
            }
            else if ((CLEAR_LEADING_BIT & bucket->slots[i].debt) < debt)
            {
                return HASHCODE_NOEXIST;
            }
            else
            {
                // Keep searching for the item to remove.
            }
        }
    }

    return HASHCODE_NOEXIST;
}


