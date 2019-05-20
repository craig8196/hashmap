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

#include <stdio.h>


typedef enum hashtype_e
{
    HBIG   = 0,
    HSMALL = 1,
    HEMPTY = 2,
}
hashtype_t;

typedef struct meta_s
{
    uint8_t       hashes[16];
    uint8_t       leaps[16];
    char          entries[];
}
meta_t;

typedef struct table_s
{
    int           len;
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
static const int LEADING_BIT = 0x80000000;
static const int MASK = 0x7FFFFFFF;
static const int HASHMAP_MIN_LEN = 8;
#ifdef TEST_HASHMAP_NOSPACE
static const int HASHMAP_MAX_LEN = 1 << 4;
#else
static const int HASHMAP_MAX_LEN = 1 << 30;
#endif
static const uint8_t EMPTY = 0xFF;

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

/*******************************************************************************
 * BEGIN FUNCTIONAL
 ******************************************************************************/

/**
 * @param n - Number of slots, must be power of 2.
 * @param slotsize - Size of one slot.
 */
static inline table_t *
table_new(int index, int n, int slotsize)
{
    table_t *t = (table_t *)malloc(sizeof(table_t) + (n * slotsize));
    
    if (NULL != t)
    {
        t->len = n;
        t->mask = n - 1;
        t->load = (n * 15) / 16;
        t->index = index;

        int i;
        meta_t *m = (meta_t *)t->table;
        for (i = 0; i < t->len; ++i)
        {
            memset(m->hashes, sizeof(m->hashes), EMPTY);
            memset(m->leaps, sizeof(m->leaps), 0);
            m = (meta_t *)((char *)m + slotsize);
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
    map->slotsize = (map->elsize * 16) + sizeof(meta_t);
    map->tabtype = 2;
    map->tablen = 0;
    map->tabshift = 32 - hashmap_log2_of_pwr2(map->tablen);
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
        table_t **tables = NULL;
        switch (map->tabtype)
        {
            case HBIG:
                tables = (table_t **)map->tables;
                break;
            case HSMALL:
                tables = (table_t **)&(map->tables);
                break;
            case HEMPTY:
                break;
        }

        int i;
        for (i = 0; i < map->tablen; ++i)
        {
            table_t *table = tables[i];
            if (NULL != table)
            {
                free(table);
            }
        }

        free(tables);
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

void *
hashmap_get(hashmap_t *map, const void *key)
{
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
    return HASHCODE_OK;
}

static inline hashcode_t 
hashmap_insert_into_table()
{
    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_insert(hashmap_t *map, const void *key, const void *val)
{
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
                hashmap_insert_into_table(map, table, hash);
            }
            break;
        case HEMPTY:
            {
                table = table_new(0, 1, map->slotsize);
                if (NULL == table)
                {
                    return HASHCODE_NOMEM;
                }
                map->tables = table;

                goto hashmap_internal_insert_jump;
            }
            break;
    }
    return HASHCODE_NOSPACE;
}

hashcode_t
hashmap_remove(hashmap_t *map, const void *key, void *kout, void *vout)
{
    return HASHCODE_OK;
}

void
hashmap_print(hashmap_t *map)
{
    printf("\n----------------------------------\n");
    printf("BEGIN HASHMAP DUMP\n");
    printf("----------------------------------\n");
    printf("BEGIN METADATA\n");
    printf("Fibonacci: %d\n", FIB);
    printf("Max length: %d\n", HASHMAP_MAX_LEN);
    printf("Empty: %x\n", (int)EMPTY);
    printf("END METADATA\n");
    printf("----------------------------------\n");
    printf("BEGIN TABLE DUMP\n");
    /*
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
    */
    printf("END TABLE DUMP\n");
    printf("----------------------------------\n");
    printf("END HASHMAP DUMP\n");
    printf("\n----------------------------------\n");
}

/*******************************************************************************
 * END
 ******************************************************************************/

