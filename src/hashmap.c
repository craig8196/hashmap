/******************************************************************************
 * Copyright (c) 2019 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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
    int           elmask;
    int           slotmask;
    int           index;
    char          slots[];
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

/******************************************************************************
 * BEGIN GLOBAL CONSTANTS
 ******************************************************************************/

/**
 * (2**32)/(Golden Ratio) ~= 2654435769
 * The two closest primes are 2654435761 and 2654435789
 */
static const uint64_t FIB = 2654435761;

/**
 * Limit the maximum table size.
 */
#ifdef TEST_HASHMAP_NOSPACE
static const int HASHMAP_MAX_LEN = 1 << 4;
#else
static const int HASHMAP_MAX_LEN = (INT_MAX/2) + 1;
#endif

/**
 * Magic number to indicate empty cell.
 */
static const uint8_t EMPTY = 0xFF;
static const uint8_t HEAD = 0x80;
//static const uint8_t MASK = 0xC0;
static const uint8_t LEAP = 0x3F;
static const uint8_t SEARCH = 0x40;

/**
 * Max distance before we switch to block searching.
 */
static const int LEAPMAX = 4;

/**
 * Number of elements per slot.
 */
static const int SLOTLEN = 16;
static const int SLOTSEARCH = 0x0000FFFF;

/******************************************************************************
 * BEGIN FUNCTIONAL
 ******************************************************************************/

static inline uint32_t
hash_fib(uint32_t hash)
{
    uint64_t bighash = FIB * (uint64_t)hash;
    return (uint32_t)(bighash) ^ (uint32_t)(bighash >> 32);
}

/**
 * @return 7bits of hash for testing.
 */
static inline uint8_t
hash_sub(uint32_t hash)
{
    uint8_t *into = (uint8_t *)(&hash);
    return ((into[0] ^ into[1] ^ into[2] ^ into[3]) & 0x7F);
//    return ((uint8_t)(hash >> 16) & 0x7F);
}

/**
 * @return Number of bits.
 */
static inline int
pop_count(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * @return The exponent needed to give you n.
 */
static inline int
log2_of_pwr2(int n)
{
    return pop_count(n - 1);
}

/**
 * @return Slot index from index.
 */
static inline int
index_slot(int index)
{
    return (index >> 4);
}

/**
 * @return Subindex into slot.
 */
static inline int
index_sub(int index)
{
    return (index & 0x0F);
}

#if 0
/**
 * @return Smallest power of 2 within range.
 */
static int
pwr2(int n, const int min, const int max)
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

/******************************************************************************
 * BEGIN SLOT
 ******************************************************************************/

/**
 * @return Integer value with bits set where there are matching entries.
 */
static inline int
slot_contains(slot_t const * const slot, const uint8_t searchhash)
{
    int result = 0;

    int i;
    for (i = 0; i < SLOTLEN; ++i)
    {
        if (slot->hashes[i] == searchhash)
        {
            result |= (1 << i);
        }
    }

    return result;
}

/**
 * @return Pointer to the key in the slot.
 */
static inline const void *
slot_key(hashmap_t const * const map, slot_t const * const slot,
         const int subindex)
{
    return (const void *)(slot->entries + (map->elsize * subindex));
}

/******************************************************************************
 * BEGIN TABLE
 ******************************************************************************/

// Forward declarations.
static inline hashcode_t 
table_insert(hashmap_t * const map, table_t *table,
             const uint32_t hash, const void *key, const void *val);

/**
 * @return Which table to choose to insert into.
 */
static inline int
table_choose(hashmap_t const * const map, const uint32_t hash)
{
    return ((hash >> 24) & map->tabshift);
}

/**
 * @param len - Power of 2.
 * @return Shift amount for HBIG table.
 */
static inline int
table_shift(int len)
{
    return (len - 1);
}

/**
 * @return Slot mask.
 */
static inline int
table_slot_mask(table_t const * const table)
{
    return table->slotmask;
//    return (((table->elmask + 1) / 16) - 1);
}

/**
 * @return Element mask.
 */
static inline int
table_el_mask(table_t const * const table)
{
    return (table->elmask);
}

/**
 * @return Index into table from hash.
 */
static inline int
table_hash_index(table_t const * const table, const uint32_t hash)
{
    return (hash & table_el_mask(table));
}

/**
 * @return The number of elements allocated for.
 */
static inline int
table_slot_len(table_t const * const table)
{
    return ((table->elmask + 1) / 16);
}

/**
 * @return Get the slot at the index.
 */
static inline slot_t *
table_slot(hashmap_t const * const map, table_t const * const table,
           const int sindex)
{
    return (slot_t *)(table->slots + (map->slotsize * sindex));
}

/**
 * @return The maximum load this table may take on.
 */
static inline int
//table_load(table_t const * const table, const int nslots)
table_load(const int nslots)
{
    // TODO make sure we don't go over maximum hashmap size, is this possible?
    // there are some quirks, the load doesn't get checked unless
    // there is some collision
    // I think just lowering the max allowed table size will allow us
    // to do this ~1Bn instead of ~2Bn
    return (nslots * 15);
}

/**
 * @brief Place the value into the table.
 * @return HASHCODE_OK.
 */
static inline hashcode_t
table_insert_value(hashmap_t *const map, table_t * const table,
                   slot_t * const slot,
                   const int subindex,
                   const uint8_t subhash, const uint8_t leap,
                   const void *key, const void *val)
{
    slot->hashes[subindex] = subhash;
    slot->leaps[subindex] = leap;
    void *el = &slot->entries[map->elsize * subindex];
    memcpy(el, key, map->keysize);
    if (map->valsize)
    {
        memcpy(el, val, map->valsize);
    }
    ++map->size;
    ++table->size;

    return HASHCODE_OK;
}

/**
 * @return True if table is considered full.
 */
static inline bool
table_is_full(table_t const * const table)
{
    return (table->size > table->load);
}

/**
 * @param n - Number of slots, must be power of 2.
 * @param slotsize - Size of one slot.
 */
static inline table_t *
table_new(const int index, const int nslots, const int slotsize)
{
    table_t *table = (table_t *)malloc(sizeof(table_t) + (nslots * slotsize));
    
    if (NULL != table)
    {
        table->size = 0;
        table->load = table_load(nslots);
        table->elmask = (nslots * 16) - 1;
        table->slotmask = nslots - 1;
        table->index = index;

        int i;
        slot_t *m = (slot_t *)table->slots;
        int slen = table_slot_len(table);
        for (i = 0; i < slen; ++i)
        {
            memset(m->hashes, EMPTY, sizeof(m->hashes));
            memset(m->leaps, 0, sizeof(m->leaps));
            m = (slot_t *)((char *)m + slotsize);
        }
    }

    return table;
}

static inline hashcode_t
table_iterate(hashmap_t * const map, table_t const * const table,
              void *ctxt, hashmap_iterate_cb_t cb)
{
    int sindex;
    int slen = table_slot_len(table);
    for (sindex = 0; sindex < slen; ++sindex)
    {
        slot_t *slot = table_slot(map, table, sindex);
        int i;
        for (i = 0; i < SLOTLEN; ++i)
        {
            const void *key = slot_key(map, slot, i);
            void *val = ((char *)key) + map->keysize;
            hashcode_t code = cb(ctxt, key, val);
            if (code)
            {
                return code;
            }
        }
    }

    return HASHCODE_OK;
}

static hashcode_t
table_iterate_readd_cb(void *ud, const void *key, void *val)
{
    hashmap_t *map = (hashmap_t *)ud;

    hashcode_t code = hashmap_insert(map, key, val);
    if (!code)
    {
        --map->size;
    }

    return code;
}

static inline hashcode_t
//table_grow_big(hashmap_t * const map, table_t **table)
table_grow_big()
{
    // TODO
    // When do we grow the table? If small, or we can't split/increase tables.
    // When do we split tables? If there is space to do so.
    // When do we increase table count? If we can't increase table. If we can't currently split but can grow then flip a coin.
    // How do we detect a bad hash? If peer table isn't within 25%??
    return HASHCODE_ERROR;
}

static hashcode_t
table_grow(hashmap_t * const map, table_t **table)
{
    static const int HSMALL_MIN = 1 << 15;
    //static const int HSMALL_MAX = 1 << 16;

    if (HASHMAP_MAX_LEN == map->size)
    {
        return HASHCODE_NOSPACE;
    }

    hashcode_t code = HASHCODE_OK;

    switch (map->tabtype)
    {
        case HBIG:
            {
                code = table_grow_big(map, table);
            }
            break;
        case HSMALL:
            {
                if (map->size <= HSMALL_MIN)
                {
                    // Just regrow this table.
                    int slen = table_slot_len(*table) * 2;
                    if (slen <= 0)
                    {
                        code = HASHCODE_NOSPACE;
                        break;
                    }
                    table_t *newtable = table_new(0, slen, map->slotsize);
                    
                    if (NULL == newtable)
                    {
                        code = HASHCODE_NOMEM;
                        break;
                    }

                    map->tables = newtable;
                    code = table_iterate(map, *table, (void *)map, table_iterate_readd_cb);
                    if (code)
                    {
                        map->size = (*table)->size;
                        map->tables = table;
                        free(newtable);
                        break;
                    }
                    else
                    {
                        free(table);
                        *table = (table_t *)map->tables;
                        break;
                    }
                }
                else
                {
                    // Upgrade to big hashmap.
                    table_t **tables = malloc(sizeof(table_t *) * 2);

                    if (NULL == tables)
                    {
                        code = HASHCODE_NOMEM;
                        break;
                    }

                    map->tabtype = HBIG;
                    map->tablen = 2;
                    map->tabshift = table_shift(map->tablen);
                    tables[0] = (table_t *)map->tables;
                    tables[1] = (table_t *)map->tables;
                    code = table_grow(map, table);
                }
            }
            break;
    }

    return code;
}

/**
 * @return Index to the next value in the chain.
 */
static inline int
table_leap(hashmap_t const * const map,
           table_t const * const table,
           slot_t const *slot,
           const int origindex,
           const int index,
           bool * const notrust)
{
    int subindex = index_sub(index);
    uint8_t leap = slot->leaps[subindex];
    if (!(leap & SEARCH))
    {
        return (index + (leap & LEAP)) & table_el_mask(table);
    }
    else
    {
        // Linear search through hashes.
        // Use same hash as previous for search efficiency.
        *notrust = true;
        uint8_t searchhash = slot->hashes[subindex];
        int sindex = index_slot(index) + ((leap & LEAP) * SLOTLEN);

        int i;
        int len = table_slot_len(table);
        int slotmask = table_slot_mask(table);

        for (i = 0; i < len; ++i)
        {
            slot = table_slot(map, table, (sindex + i) & slotmask);
            int searchmap = slot_contains(slot, searchhash);
            while (searchmap)
            {
                int currsubindex = __builtin_ffs(searchmap) - 1;
                const void *key = slot_key(map, slot, currsubindex);
                uint32_t currhash = hash_fib(map->hash_cb(key));
                int currindex = table_hash_index(table, currhash);
                if (origindex == currindex && currindex != index)
                {
                    return sindex + currsubindex;
                }
                searchmap = searchmap & (~(1 << currsubindex));
            }
        }

        // We should never get here.
        return 0;
    }
}

/**
 * @return Index of the last element in the list.
 */
static inline int
table_find_end(hashmap_t const * const map,
               table_t const * const table,
               int origindex,
               int index)
{
    for (;;)
    {
        slot_t *slot = table_slot(map, table, index);
        int subindex = index_sub(index);
        if (0 == (slot->leaps[subindex] & LEAP))
        {
            return index;
        }

        bool scratch;
        index = table_leap(map, table, slot, origindex, index, &scratch);
    }

    // We should never get here.
    return 0;
}

/**
 * @brief Find the next available slot and place the element.
 */
static hashcode_t
table_emplace(hashmap_t * const map, table_t *table, slot_t *currslot,
              const uint32_t hash, const uint8_t subhash,
              const void *key, const void *val,
              int index)
{
    if (table_is_full(table))
    {
        hashcode_t code = table_grow(map, &table);
        if (code)
        {
            return code;
        }
        else
        {
            return table_insert(map, table, hash, key, val);
        }
    }
    else
    {
        // Find an empty slot.
        int i;
        int sindex;
        int subindex;
        int testindex = (index + 1) & table_el_mask(table);
        slot_t *slot = NULL;
        for (i = 1; i < LEAPMAX; ++i)
        {
            sindex = index_slot(testindex);
            subindex = index_sub(testindex);
            slot = table_slot(map, table, sindex);

            if (EMPTY == slot->hashes[subindex])
            {
                currslot->leaps[index_sub(index)] =
                    (HEAD & currslot->leaps[index_sub(index)])
                    | (LEAP & (uint8_t)i);
                return table_insert_value(map, table, slot, subindex, subhash, 0, key, val);
            }

            testindex = (index + i) & table_el_mask(table);
        }


        int slotlen = table_slot_len(table);
        int slotmask = table_slot_mask(table);
        for (i = 1, sindex = index_slot(index); i <= slotlen; ++i)
        {
            slot = table_slot(map, table, (sindex + i) & slotmask);
            int searchmap = slot_contains(slot, EMPTY);
            if (searchmap)
            {
                int currsubindex = __builtin_ffs(searchmap) - 1;
                int leap = i < LEAPMAX ? i : (LEAPMAX + 1);
                currslot->leaps[index_sub(index)] = 
                    (HEAD & currslot->leaps[index_sub(index)])
                    | (LEAP & leap)
                    | (SEARCH);
                return table_insert_value(map, table, slot, currsubindex, currslot->hashes[index_sub(index)], 0, key, val);
            }
        }

        // We should never get here.
        return HASHCODE_ERROR;
    }
}

static inline int
table_find_prev_to_index(hashmap_t const * const map,
                         table_t const * const table,
                         slot_t const * const slot,
                         int origindex,
                         int searchindex)
{
    int index = origindex;
    bool scrap;
    for (;;)
    {
        int nextindex = table_leap(map, table, slot, origindex, index, &scrap);
        if (nextindex == searchindex)
        {
            return index;
        }
        index = nextindex;
    }

    return 0;
}

/**
 * @brief Relocate the given value.
 */
static hashcode_t
table_re_emplace(hashmap_t * const map,
                 table_t *table,
                 slot_t *slot,
                 int origindex,
                 const uint32_t hash,
                 const uint8_t subhash,
                 const void *key,
                 const void *val)
{
    if (table_is_full(table))
    {
        hashcode_t code = table_grow(map, &table);
        if (code)
        {
            return code;
        }
        else
        {
            return table_insert(map, table, hash, key, val);
        }
    }
    else
    {
        // Find the entry pointing to our value.
        const void *currkey = slot_key(map, slot, index_sub(origindex));
        uint32_t currhash = map->hash_cb(currkey);
        int headindex = table_hash_index(table, hash);
        int previndex = table_find_prev_to_index(map, table, slot, headindex, origindex);
        // Remove entry from linked list.
        uint8_t currleap = slot->leaps[index_sub(origindex)];
        slot_t *prevslot = table_slot(map, table, index_slot(previndex));
        uint8_t prevleap = prevslot->leaps[index_sub(previndex)];
        if (0 == (currleap & LEAP))
        {
            // Emplace key/val since we're relocating the tail.
            const void *currval = (const char *)currkey + map->keysize;
            hashcode_t code = table_emplace(map, table, slot, currhash, hash_sub(currhash), currkey, currval, previndex);
            if (code)
            {
                // No changes made yet.
                // TODO undo any changes
                return code;
            }
        }
        else
        {
            bool badsearch = false;
            if (!(prevleap & SEARCH) && !(currleap & SEARCH))
            {
                int dist = (int)(prevleap & LEAP) + (int)(currleap & LEAP);
                if (dist < LEAPMAX)
                {
                    // Good, we don't have to resort to inefficient searching.
                    prevslot->leaps[index_sub(previndex)] =
                        (prevslot->leaps[index_sub(previndex)] & HEAD)
                        | ((uint8_t)dist);
                }
                else
                {
                    badsearch = true;
                }
            }

            int nextindex = table_leap(map, table, slot, headindex, origindex, &badsearch);

            if (badsearch)
            {
                int prevsindex = index_slot(previndex);
                int nextsindex = index_slot(nextindex);
                int dist = nextsindex < prevsindex ?
                    ((nextsindex + table_slot_len(table)) - prevsindex)
                    : (nextsindex - prevsindex);
                uint8_t newleap;
                if (dist < LEAPMAX)
                {
                    newleap = SEARCH | ((uint8_t)dist);
                }
                else
                {
                    newleap = SEARCH | (LEAPMAX - 1);
                }

                // Good, we can 
                prevslot->leaps[index_sub(previndex)] =
                    (prevslot->leaps[index_sub(previndex)] & HEAD)
                    | newleap;
            }

            // Find end of list.
            int emplaceindex = table_find_end(map, table, headindex, nextindex);

            // Emplace key/val.
            const void *currval = (const char *)key + map->keysize;
            slot = table_slot(map, table, index_slot(emplaceindex));
            hashcode_t code = 
                table_emplace(map, table, slot, currhash, hash_sub(currhash),
                              currkey, currval, emplaceindex);
            if (code)
            {
                return code;
            }
        }

        // Place new key/val
        return table_insert_value(map, table, slot, index_sub(origindex), subhash, HEAD, key, val);
    }
}

static inline void *
table_get(hashmap_t const * const map, table_t const * const table,
          const uint32_t hash, const void *key)
{
    int origindex = table_hash_index(table, hash);
    int index = origindex;
    uint8_t subhash = hash_sub(hash);
    bool first = true;
    bool notrust = false;

    for (;;)
    {
        int subindex = index_sub(index);
        slot_t *slot = table_slot(map, table, index_slot(index));

        if (first)
        {
            if (EMPTY == slot->hashes[subindex])
            {
                return NULL;
            }

            if (!(slot->leaps[subindex] & HEAD))
            {
                return NULL;
            }

            first = false;
        }

        if (notrust || (subhash == slot->hashes[subindex]))
        {
            // Maybe already exists.
            const void *key2 = slot_key(map, slot, subindex);
            if (map->eq_cb(key, key2))
            {
                void *val = ((char *)key2) + map->keysize;
                return val;
            }
        }

        if (0 == (slot->leaps[subindex] & LEAP))
        {
            return NULL;
        }

        notrust = false;
        index = table_leap(map, table, slot, origindex, index, &notrust);
    }

    return NULL;
}

static inline hashcode_t 
table_insert(hashmap_t * const map, table_t *table,
             const uint32_t hash, const void *key, const void *val)
{
    int origindex = table_hash_index(table, hash);
    int index = origindex;
    const uint8_t subhash = hash_sub(hash);
    bool first = true;
    bool notrust = false;

    for (;;)
    {
        int sindex = index_slot(index);
        int subindex = index_sub(index);
        slot_t *slot = table_slot(map, table, sindex);

        if (first)
        {
            if (EMPTY == slot->hashes[subindex])
            {
                // Empty, the optimal case.
                return table_insert_value(map, table, slot, subindex, subhash, HEAD, key, val);
            }

            if (!(slot->leaps[subindex] & HEAD))
            {
                // Worst case scenario.
                // Cons of linked list, relocate.
                return table_re_emplace(map, table, slot, index, hash, subhash, key, val);
            }

            first = false;
        }

        if (notrust || subhash == slot->hashes[subindex])
        {
            // Maybe already exists.
            const void *key2 = slot_key(map, slot, subindex);
            if (map->eq_cb(key, key2))
            {
                return HASHCODE_EXIST;
            }
        }

        if (0 == (slot->leaps[subindex] & LEAP))
        {
            return table_emplace(map, table, slot, hash, subhash, key, val, index);
        }

        notrust = false;
        index = table_leap(map, table, slot, origindex, index, &notrust);
    }

    return HASHCODE_NOSPACE;
}

/******************************************************************************
 * BEGIN HASHMAP
 ******************************************************************************/

/** INIT/DESTROY FUNCTIONS **/

hashcode_t
hashmap_init(hashmap_t * const map,
             const int keysize,
             const int valsize,
             const hashmap_hash_cb_t hash_cb,
             const hashmap_eq_cb_t eq_cb)
{
    map->size = 0;
    map->keysize = keysize;
    map->valsize = valsize;
    map->elsize = keysize + valsize;
    map->slotsize = (map->elsize * 16) + sizeof(slot_t);
    map->tabtype = 2;
    map->tablen = 0;
    map->tabshift = table_shift(map->tablen);
    map->tables = NULL;
    map->hash_cb = hash_cb;
    map->eq_cb = eq_cb;

    return HASHCODE_OK;
}

void
hashmap_destroy(hashmap_t * const map)
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

/** QUERY FUNCTIONS **/

int
hashmap_size(hashmap_t const * const map)
{
    return map->size;
}

bool
hashmap_empty(hashmap_t const * const map)
{
    return !(map->size);
}

void *
hashmap_get(hashmap_t const * const map, const void *key)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = ((table_t **)map->tables)[table_choose(map, hash)];
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

    return table_get(map, table, hash, key);
}

void *
hashmap_getkey(hashmap_t const * const map, const void *key)
{
    char *val = (char *)hashmap_get(map, key);

    if (NULL != val)
    {
        return (void *)(val - map->keysize);
    }

    return (void *)val;
}


bool
hashmap_contains(hashmap_t const * const map, const void *key)
{
    return !(NULL == hashmap_get(map, key));
}

hashcode_t
hashmap_iterate(hashmap_t const * const map, void *ud,
                const hashmap_iterate_cb_t iter_cb)
{
    table_t **tables = NULL;

    switch (map->tabtype)
    {
        case HBIG:
            {
                tables = (table_t **)map->tables;
            }
            break;
        case HSMALL:
            {
                tables = (table_t **)&(map->tables);
            }
            break;
        case HEMPTY:
            {
            }
            break;
    }

    int tindex;
    for (tindex = 0; tindex < map->tablen; ++tindex)
    {
        table_t *table = tables[tindex];

        int sindex;
        int slen = table_slot_len(table);
        for (sindex = 0; sindex < slen; ++sindex)
        {
            slot_t *slot = table_slot(map, table, sindex);
            int searchmap = SLOTSEARCH & (~(slot_contains(slot, EMPTY)));
            while (searchmap)
            {
                int subindex = __builtin_ffs(searchmap) - 1;
                const void *key = slot_key(map, slot, subindex);
                void *val = ((char *)key) + map->keysize;

                hashcode_t code = iter_cb(ud, key, val);
                if (code)
                {
                    return code;
                }
                
                searchmap = searchmap & (~(1 << subindex));
            }
        }
    }

    return HASHCODE_OK;
}

/** MOD FUNCTIONS **/

hashcode_t
hashmap_insert(hashmap_t * const map, const void *key, const void *val)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = ((table_t **)map->tables)[table_choose(map, hash)];
            }
            break;
        case HSMALL:
            {
                table = (table_t *)map->tables;
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
                map->tabtype = HSMALL;
                map->tablen = 1;
                map->tabshift = table_shift(map->tablen);
            }
            break;
    }

    return table_insert(map, table, hash, key, val);
}

hashcode_t
hashmap_remove(hashmap_t * map, const void *key,
               void * kout, void * vout)
// TODO
//hashmap_remove(hashmap_t * const map, const void *key,
//               void * const kout, void * const vout)
{
    // TODO
    map = map; key = key; kout = kout; vout = vout;
    return HASHCODE_ERROR;
}

/** DEBUG FUNCTIONS **/

#define LINE "----------------------------------\n"
#define DELIM "*\n"

static void
print_key(char const * const key, const int size)
{
    int i;
    for (i = 0; i < size; ++i)
    {
        printf("%02X", key[i] & 0xFF);
    }
}

static void
table_print_slots(hashmap_t const * const map, table_t const * const table)
{
    int sindex;
    int slen = table_slot_len(table);
    for (sindex = 0; sindex < slen; ++sindex)
    {
        slot_t *slot = table_slot(map, table, sindex);
        int i;
        for (i = 0; i < SLOTLEN; ++i)
        {
            printf("H:0x%02X|L:0x%02X",
                (unsigned int)slot->hashes[i],
                (unsigned int)slot->leaps[i]);
            if (EMPTY != slot->hashes[i])
            {
                if (HEAD & slot->leaps[i])
                {
                    printf(" (head");
                }
                else
                {
                    printf(" (cons");
                }

                if (0 == (LEAP & slot->leaps[i]))
                {
                    printf(",tail");
                }

                printf(") 0x");

                char const *key = (char const *)slot_key(map, slot, i);
                print_key(key, map->keysize);
            }
            printf("\n");
        }

        if ((sindex + 1) < slen)
        {
            printf(DELIM);
        }
    }
}

void
hashmap_print(hashmap_t const * const map)
{
    int64_t octets = sizeof(hashmap_t);
    printf("\n"LINE);
    printf("METADATA\n");
    printf(LINE);
    printf("Fibonacci: %"PRIu64"\n", FIB);
    printf("Max length: %d\n", HASHMAP_MAX_LEN);
    printf("Empty: 0x%X\n", (int)EMPTY);
    printf(LINE);
    printf("HASHMAP\n");
    printf(LINE);
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
    printf(LINE);
    printf("TABLE DUMP\n");
    printf(LINE);

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
        printf("TABLE\n");
        printf(LINE);

        table_t *table = tables[i];
        if (table->index == i)
        {
            printf("Table: %d\n", table->index);
            printf("Size: %d\n", table->size);
            printf("Load: %d\n", table->load);
            printf("Slot Len: %d\n", table_slot_len(table));
            printf("El Mask: 0x%X\n", table_el_mask(table));
            printf("Slot Mask: 0x%X\n", table_slot_mask(table));
            octets += sizeof(table_t);
            octets += map->slotsize * table_slot_len(table);
            printf(LINE);
            printf("SLOTS\n");
            printf(LINE);
            table_print_slots(map, table);
        }
        else
        {
            printf("Table: %d (duplicate)\n", table->index);
            printf(LINE);
        }
    }

    printf(LINE);
    printf("Total Octets: %"PRIu64"\n", octets);
    printf("END\n");
    printf(LINE);
}

/******************************************************************************
 * END
 ******************************************************************************/

