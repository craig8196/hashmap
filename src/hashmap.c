/*******************************************************************************
 * Copyright (c) 2019 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#include "hashmap.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <inttypes.h>
#include <stdio.h>


typedef enum hashtype_e
{
    HBIG   = 0,
    HSMALL = 1,
    HEMPTY = 2,
}
hashtype_t;

typedef struct slot_s
{
    uint8_t       hashes[16];
    uint8_t       leaps[16];
    char          entries[];
}
slot_t;

typedef struct table_s
{
    int           size;
    int           load;
    int           mask;
    int           index;
    char          table[];
}
table_t;


#if 0
#include <emmintrin.h>
broadcast word8 value
__m128i _mm_set1_epi8 (char a)
compare word8 value
__mm_cmpeq_epi8(__m128i a, __m128i b)
take first bit in each register
int __mm_movemask_epi8(__m128i a)
get index of first
_bit_scan_forward()
#endif

/*******************************************************************************
 * BEGIN GLOBAL CONSTANTS
 ******************************************************************************/

/**
 * (2**32)/(Golden Ratio) ~= 2654435769
 * The two closest primes are 2654435761 and 2654435789
 */
static const uint32_t FIB = 2654435761;

/**
 * Limit the maximum table size.
 */
#ifdef TEST_HASHMAP_NOSPACE
static const int HASHMAP_MAX_LEN = 1 << 4;
#else
static const int HASHMAP_MAX_LEN = INT_MAX;
#endif
/**
 * Magic number to indicate empty cell.
 */
static const uint8_t EMPTY = 0xFF;
static const uint8_t HEAD = 0x80;
static const uint8_t MASK = 0xC0;


/*******************************************************************************
 * BEGIN FUNCTIONAL
 ******************************************************************************/

/**
 * @return 7bits of hash for testing.
 */
static inline uint8_t
hashmap_subhash(uint32_t hash)
{
    return ((uint8_t)(hash >> 16) & 0x7F);
}

/**
 * @return Number of bits.
 */
static inline int
hashmap_pop_count(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * @return The exponent needed to give you n.
 */
static int
hashmap_log2_of_pwr2(int n)
{
    return hashmap_pop_count(n - 1);
}

#if 0
/**
 * @return Smallest power of 2 within range.
 */
static int
hashmap_pwr2(int n, const int min, const int max)
{
    if (n <= min)
    {
        return min;
    }
    else if (n >= max)
    {
        return max;
    }
    else
    {
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        ++n;
        return n - 1;
    }
}
#endif

/*******************************************************************************
 * BEGIN FUNCTIONAL
 ******************************************************************************/

/**
 * @param n - Number of slots, must be power of 2.
 * @param slotsize - Size of one slot.
 */
static inline table_t *
hashmap_table_new(int index, int n, int slotsize)
{
    table_t *t = (table_t *)malloc(sizeof(table_t) + (n * slotsize));
    
    if (NULL != t)
    {
        t->size = 0;
        t->mask = n - 1;
        t->load = (n * 15);
        t->index = index;

        int i;
        slot_t *m = (slot_t *)t->table;
        for (i = 0; i < (t->mask + 1); ++i)
        {
            memset(m->hashes, EMPTY, sizeof(m->hashes));
            memset(m->leaps, 0, sizeof(m->leaps));
            m = (slot_t *)((char *)m + slotsize);
        }
    }

    return t;
}

/*******************************************************************************
 * BEGIN IMPLEMENTATION
 ******************************************************************************/

hashcode_t
hashmap_init(hashmap_t *map,
             int keysize,
             int valsize,
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb)
{
    map->size = 0;
    map->keysize = keysize;
    map->valsize = valsize;
    map->elsize = keysize + valsize;
    map->slotsize = (map->elsize * 16) + sizeof(slot_t);
    map->tabtype = 2;
    map->tablen = 0;
    map->tabshift = 0;
    map->tables = NULL;
    map->hash_cb = hash_cb;
    map->eq_cb = eq_cb;

    return HASHCODE_OK;
}

void
hashmap_destroy(hashmap_t *map)
{
    if (NULL != map->tables)
    {
        switch (map->tabtype)
        {
            case HBIG:
                {
                    table_t **tables = (table_t **)map->tables;
                    int i;
                    for (i = 0; i < map->tablen; ++i)
                    {
                        table_t *table = tables[i];
                        if (NULL != table)
                        {
                            free(table);
                        }
                    }
                }
                break;
            case HSMALL:
                break;
            case HEMPTY:
                break;
        }
        if (NULL != map->tables)
        {
            free(map->tables);
        }
    }

    memset(map, 0, sizeof(hashmap_t));
}

int
hashmap_size(hashmap_t *map)
{
    return map->size;
}

bool
hashmap_empty(hashmap_t *map)
{
    return !(map->size);
}

static inline void *
hashmap_table_get(hashmap_t *map, table_t *table, uint32_t hash, const void *key)
{
    int index = hash & table->mask;
    uint8_t subhash = hashmap_subhash(hash);

    do
    {
        int sindex = index >> 4;
        int subindex = index & 0x0F;
        slot_t *slot = (slot_t *)(table->table + (map->slotsize * sindex));
        if (subhash == slot->hashes[subindex])
        {
            const void *key2 = slot->entries + (map->elsize * subindex);
            if (map->eq_cb(key, key2))
            {
                void *val = (void *)((char *)key2 + map->keysize);
                return val;
            }
        }
        else if (EMPTY == slot->hashes[subindex])
        {
            return NULL;
        }
    }
    while (false);

    return NULL;
}

void *
hashmap_get(hashmap_t *map, const void *key)
{
    table_t *table = NULL;
    uint32_t hash = FIB * map->hash_cb(key);

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = ((table_t **)map->tables)[hash >> map->tabshift];
            }
            break;
        case HSMALL:
            {
                table = (table_t *)map->tables;
            }
            break;
        case HEMPTY:
            {
                return NULL;
            }
            break;
    }

    return hashmap_table_get(map, table, hash, key);
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
    map = map; ud = ud; iter_cb = iter_cb;
    return HASHCODE_OK;
}

static hashcode_t
hashmap_table_grow(hashmap_t *map, table_t *table, uint32_t hash)
{
    map = map; table = table; hash = hash;
    return HASHCODE_ERROR;
}

static inline void
hashmap_table_insert_value(hashmap_t *map, table_t *table, slot_t *slot,
                           int subindex, uint8_t subhash,
                           const void *key, const void *val)
{
    slot->hashes[subindex] = subhash;
    slot->leaps[subindex] = HEAD;
    void *el = &slot->entries[map->elsize * subindex];
    memcpy(el, key, map->keysize);
    if (map->valsize)
    {
        memcpy(el, val, map->valsize);
    }
    ++map->size;
    ++table->size;
}

static inline int
hashmap_table_find_empty(int *index)
{
    index = index;
    return 0;
}

static inline hashcode_t 
hashmap_table_insert(hashmap_t *map, table_t *table, uint32_t hash, const void *key, const void *val)
{
    map = map; table = table; hash = hash;

    // Check if there is room.
    if (table->size == table->load)
    {
        hashcode_t code;
        if ((code = hashmap_table_grow(map, table, hash)))
        {
            return code;
        }
    }

    int i;
    int index = hash & table->mask;
    uint8_t subhash = hashmap_subhash(hash);

    for (i = 0; i < table->load; ++i)
    {
        int sindex = index >> 4;
        int subindex = index & 0x0F;
        slot_t *slot = (slot_t *)(table->table + (map->slotsize * sindex));
        if (subhash == slot->hashes[subindex])
        {
            const void *key2 = slot->entries + (map->elsize * subindex);
            if (map->eq_cb(key, key2))
            {
                return HASHCODE_EXIST;
            }
        }
        else if (EMPTY == slot->hashes[subindex])
        {
            hashmap_table_insert_value(map, table, slot, sindex, subhash, key, val);
            return HASHCODE_OK;
        }

        if (0 == (slot->leaps[subindex] & MASK))
        {
            int leap = hashmap_table_find_empty(&index);
            slot->leaps[subindex] = leap;
            sindex = index >> 4;
            subindex = index & 0x0F;
            slot = (slot_t *)(table->table + (map->slotsize * sindex));
            hashmap_table_insert_value(map, table, slot, sindex, subhash, key, val);
            return HASHCODE_OK;
        }

        sindex = (sindex + 1) & table->mask;
    }

    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_insert(hashmap_t *map, const void *key, const void *val)
{
    map = map; val = val; key = key;
    table_t *table = NULL;
    uint32_t hash = FIB * map->hash_cb(key);
    switch (map->tabtype)
    {
        case HBIG:
            {
                table = ((table_t **)map->tables)[hash >> map->tabshift];
                goto hashmap_internal_insert_jump;
            }
            break;
        case HSMALL:
            {
                table = (table_t *)map->tables;
                hashmap_internal_insert_jump:
                return hashmap_table_insert(map, table, hash, key, val);
            }
            break;
        case HEMPTY:
            {
                table = hashmap_table_new(0, 1, map->slotsize);
                if (NULL == table)
                {
                    return HASHCODE_NOMEM;
                }
                map->tables = table;
                map->tabtype = HSMALL;
                map->tablen = 1;
                map->tabshift = 32 - hashmap_log2_of_pwr2(map->tablen);

                goto hashmap_internal_insert_jump;
            }
            break;
    }
    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_remove(hashmap_t *map, const void *key, void *kout, void *vout)
{
    // TODO
    map = map; key = key; kout = kout; vout = vout;
    return HASHCODE_ERROR;
}

void
hashmap_print(hashmap_t *map)
{
    int64_t octets = sizeof(hashmap_t);
    printf("\n----------------------------------\n");
    printf("METADATA\n");
    printf("----------------------------------\n");
    printf("Fibonacci: %u\n", FIB);
    printf("Max length: %d\n", HASHMAP_MAX_LEN);
    printf("Empty: 0x%X\n", (int)EMPTY);
    printf("----------------------------------\n");
    printf("HASHMAP\n");
    printf("----------------------------------\n");
    printf("Size: %d\n", map->size);
    printf("Key Size: %d\n", map->keysize);
    printf("Val Size: %d\n", map->valsize);
    printf("El Size: %d\n", map->elsize);
    printf("Slot Size: %d\n", map->slotsize);
    switch (map->tabtype)
    {
        case HBIG:
            {
                printf("Table type: BIG\n");
            }
            break;
        case HSMALL:
            {
                printf("Table type: SMALL\n");
            }
            break;
        case HEMPTY:
            {
                printf("Table type: EMPTY\n");
            }
            break;
    }
    printf("Table Shift: %d\n", map->tabshift);
    printf("Table Count: %d\n", map->tablen);
    printf("----------------------------------\n");
    printf("TABLE DUMP\n");
    printf("----------------------------------\n");

    table_t **tables = NULL;

    switch (map->tabtype)
    {
        case HBIG:
            {
                tables = (table_t **)map->tables;
                octets += map->tablen * sizeof(table_t *);
            }
            break;
        case HSMALL:
            {
                tables = (table_t **)&(map->tables);
            }
            break;
        case HEMPTY:
            {
                printf("EMPTY\n");
            }
            break;
    }

    int i;
    for (i = 0; i < map->tablen; ++i)
    {
        table_t *table = tables[i];
        if (table->index == i)
        {
            printf("Table: %d\n", table->index);
            printf("Size: %d\n", table->size);
            printf("Load: %d\n", table->load);
            printf("Mask: 0x%X\n", table->mask);
            octets += sizeof(table_t) + (map->slotsize * (table->mask + 1));
        }
        else
        {
            printf("Duplicate of table: %d\n", table->index);
        }
    }

    printf("----------------------------------\n");
    printf("Total Octets: %"PRIu64"\n", octets);
    printf("END\n");
    printf("----------------------------------\n");
}

/*******************************************************************************
 * END
 ******************************************************************************/

