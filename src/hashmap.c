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

#include <emmintrin.h>

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
//static const int LEAPMAX = 1 << 4;
static const int LEAPMAX = 1 << 6;

/**
 * Minimum BIG table allocation.
 */
static const int HBIGMIN = 1 << 13;

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
#if 0
    // Another variant I would like to try.
//    hash ^= (hash >> ((sizeof(hash) * 8)/2));
//    hash ^= (hash >> ((sizeof(hash) * 8)/4));
//    return (hash & 0x7F);
    // For when I experiment with moving the EMPTY check to leaps
//    return (hash & 0xFF);
#endif
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

/**
 * @return Combined index.
 */
static inline int
index_from(int sindex,
           int subindex)
{
    return ((sindex * SLOTLEN) + subindex);
}

/**
 * @return Peer index when indexing into table of tables.
 */
static inline int
index_peer(int index, int len)
{
    return ((len >> 1) ^ index);
}

/**
 * @return Distance from tail index.
 */
static inline int
index_loc(const int h,
          const int t,
          const int e,
          const int len,
          const int mask)
{
    // I think this works.
    // Len is power of two and mask is the mask for modulo.
    // I'm not sure this works under subtraction...
    // (A + B) mod M = ((A mod M) + (B mod M)) mod M
    // Find tail delta from head: (tail + length) - head modulo len
    // Find empty delta from head: (empty + length) - head modulo len
    // Return difference of empty and tail deltas.
    return (((e + len) - h) & mask) - (((t + len) - h) & mask);
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

/**
 * @brief Quickly copy data.
 */
static inline void
copydata(void *to, const void *from, int size)
{
    switch (size)
    {
        case 0:
            break;
        case sizeof(uint8_t):
            *((uint8_t *)to) = *((uint8_t *)from);
            break;
        case sizeof(uint16_t):
            *((uint16_t *)to) = *((uint16_t *)from);
            break;
        case sizeof(uint32_t):
            *((uint32_t *)to) = *((uint32_t *)from);
            break;
        case sizeof(uint64_t):
            *((uint64_t *)to) = *((uint64_t *)from);
            break;
        default:
            memcpy(to, from, size);
            break;
    }
}

/******************************************************************************
 * BEGIN SLOT
 ******************************************************************************/

/**
 * @return Integer value with bits set where there are matching entries.
 */
static inline int
slot_contains(slot_t const * const slot,
              const uint8_t searchhash)
{
#if defined __SSE2__
    // Documentation:
    // https://software.intel.com/sites/landingpage/IntrinsicsGuide/
    __m128i first = _mm_set1_epi8((char)searchhash);
    __m128i second = _mm_loadu_si128((__m128i *)(slot->hashes));
    return _mm_movemask_epi8(_mm_cmpeq_epi8(first, second));
#else
    // Slower implementation available if we don't have SSE instructions.
    // TODO this can even be optimized
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
#endif
}

/**
 * @return Integer value with bits set where there are matches after subindex.
 */
static inline int
slot_contains_after(slot_t const * const slot,
                    const uint8_t searchhash,
                    int subindex)
{
    return (slot_contains(slot, searchhash) & ~((1 << subindex) - 1));
}

/**
 * @return Integer value with bits set where there are matches before subindex.
 */
static inline int
searchmap_limit_before(int searchmap,
                       int subindex)
{
    return (searchmap & ((1 << subindex) - 1));
}

/**
 * @param searchmap - Non-zero map.
 * @return Index from search map.
 */
static inline int
searchmap_next(int searchmap)
{
    return (__builtin_ffs(searchmap) - 1);
}

/**
 * @param subindex - In range [0, 15].
 * @return New search map with index cleared.
 */
static inline int
searchmap_clear(int searchmap, int subindex)
{
    return (searchmap & (~(1 << subindex)));
}


/**
 * @return Pointer to the key in the slot.
 */
static inline const void *
slot_key(hashmap_t const * const map,
         slot_t const * const slot,
         const int subindex)
{
    return (const void *)(slot->entries + (map->elsize * subindex));
}

/**
 * @return Pointer to the val in the slot.
 */
static inline const void *
slot_val(hashmap_t const * const map,
         slot_t const * const slot,
         const int subindex)
{
    return (const void *)(slot->entries + (map->elsize * subindex) + map->keysize);
}

/**
 * @brief Copy out the data.
 */
static inline void
slot_export(hashmap_t const * const map,
            slot_t const * const slot,
            void * const kout,
            void * const vout,
            int subindex)
{
    if (kout)
    {
        const void *key = slot_key(map, slot, subindex);
        copydata(kout, key, map->keysize);
    }

    if (vout)
    {
        const void *val = slot_val(map, slot, subindex);
        copydata(vout, val, map->valsize);
    }
}

/**
 * @brief Perform the necessary cast.
 */
static inline table_t **
map_tables(hashmap_t const * const map)
{
    return (table_t **)map->tables;
}

/******************************************************************************
 * BEGIN TABLE
 ******************************************************************************/

// Forward declarations.
static inline hashcode_t 
table_insert(hashmap_t * const map,
             table_t *table,
             const uint32_t hash,
             const void *key,
             const void *val);

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
table_hash_index(table_t const * const table,
                 const uint32_t hash)
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
table_slot(hashmap_t const * const map,
           table_t const * const table,
           const int sindex)
{
    return (slot_t *)(table->slots + (map->slotsize * sindex));
}

/**
 * @return Number of entries in the table.
 */
static inline int
table_len(table_t const * const table)
{
    return (table_slot_len(table) * SLOTLEN);
}

/**
 * @return Number of slots between given; zero otherwise.
 */
static inline int
table_slot_count(table_t const * const table,
                 int index1,
                 int index2)
{
    return (((index2 <= index1)
             ? (index2 + table_len(table)) - index1
             : index2 - index1)
            / SLOTLEN);
}

/**
 * @return Number of entries between, including last index.
 */
static inline int
table_index_dist(table_t const * const table,
                 int index1,
                 int index2)
{
    return ((index2 < index1)
            ? (index2 + table_len(table)) - index1
            : index2 - index1);
}

/**
 * @brief Copy the entry from one point in the table to another.
 */
static inline void
table_copy_entry(hashmap_t const * const map,
                 table_t * const table,
                 int fromindex,
                 int toindex)
{
    int fsub = index_sub(fromindex);
    int tsub = index_sub(toindex);
    slot_t *fslot = table_slot(map, table, fsub);
    slot_t *tslot = table_slot(map, table, tsub);
    const void *f = slot_key(map, fslot, fsub);
    void *t = (void *)slot_key(map, tslot, tsub);
    copydata(t, f, map->elsize);
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
table_place(hashmap_t *const map,
            table_t * const table,
            slot_t * const slot,
            const int subindex,
            const uint8_t subhash,
            const uint8_t leap,
            const void *key,
            const void *val)
{
    slot->hashes[subindex] = subhash;
    slot->leaps[subindex] = leap;
    char *el = (char *)slot_key(map, slot, subindex);
    copydata(el, key, map->keysize);
    copydata((char *)el + map->keysize, val, map->valsize);
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
 * @brief Clear all entries in the table.
 */
static inline void
table_clear(table_t * const table,
            const int slotsize)
{
    table->size = 0;
    int i;
    slot_t *m = (slot_t *)table->slots;
    int slen = table_slot_len(table);
    for (i = 0; i < slen; ++i)
    {
        memset(m->hashes, EMPTY, sizeof(m->hashes));
        m = (slot_t *)((char *)m + slotsize);
    }
}

/**
 * @param n - Number of slots, must be power of 2.
 * @param slotsize - Size of one slot.
 */
static inline table_t *
table_new(const int index,
          const int nslots,
          const int slotsize)
{
    table_t *table = (table_t *)malloc(sizeof(table_t) + (nslots * slotsize));
    
    if (NULL != table)
    {
        table->load = table_load(nslots);
        table->elmask = (nslots * 16) - 1;
        table->slotmask = nslots - 1;
        table->index = index;
        table_clear(table, slotsize);
    }

    return table;
}

static inline hashcode_t
table_iterate(hashmap_t * const map,
              table_t const * const table,
              void *ctxt,
              hashmap_iterate_cb_t cb)
{
    int sindex;
    int slen = table_slot_len(table);
    for (sindex = 0; sindex < slen; ++sindex)
    {
        slot_t *slot = table_slot(map, table, sindex);
        int i;
        for (i = 0; i < SLOTLEN; ++i)
        {
            if (EMPTY != slot->hashes[i])
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
    }

    return HASHCODE_OK;
}

static hashcode_t
table_iterate_readd_cb(void *ud,
                       const void *key,
                       void *val)
{
    hashmap_t *map = (hashmap_t *)ud;
    return hashmap_insert(map, key, val);
}

static inline hashcode_t
table_grow_big(hashmap_t * const map,
               table_t **table,
               const uint32_t hash)
{
    hashcode_t code = HASHCODE_OK;

    int origmapsize = map->size;
    int index = (*table)->index;
    int peer = index_peer(index, map->tablen);
    table_t **tables = map_tables(map);
    do
    {
        if (tables[index] == tables[peer])
        {
            // Split current table if it is splitable.
            table_t *indextable = table_new(index, table_slot_len(*table), map->slotsize);
            if (NULL == indextable)
            {
                code = HASHCODE_NOMEM;
                break;
            }
            table_t *peertable = table_new(peer, table_slot_len(*table), map->slotsize);
            if (NULL == peertable)
            {
                free(indextable);
                code = HASHCODE_NOMEM;
                break;
            }
            tables[index] = indextable;
            tables[peer] = peertable;

            map->size -= (*table)->size;
            code = table_iterate(map, *table, (void *)map, table_iterate_readd_cb);
            map->size = origmapsize;
            if (code)
            {
                tables[index] = *table;
                tables[peer] = *table;
                free(peertable);
                free(indextable);
            }
            else
            {
                free(*table);
                *table = tables[table_choose(map, hash)];
            }
            break;
        }
#if 0
        // I need to develop a good heuristic for this case.
        else if (map_is_)
        {
        }
#endif
        else
        {
            // Increase the size of the current table.
            int slen = table_slot_len(*table) * 2;
            if (slen <= 0)
            {
                code = HASHCODE_NOSPACE;
                break;
            }
            table_t *newtable = table_new(index, slen, map->slotsize);
            
            if (NULL == newtable)
            {
                code = HASHCODE_NOMEM;
                break;
            }

            tables[index] = newtable;
            map->size -= (*table)->size;
            code = table_iterate(map, *table, (void *)map, table_iterate_readd_cb);
            map->size = origmapsize;
            if (code)
            {
                map->size = (*table)->size;
                tables[index] = *table;
                free(newtable);
            }
            else
            {
                free(*table);
                *table = newtable;
            }
            break;
        }
    }
    while (false);

    return code;
}

static hashcode_t
table_grow(hashmap_t * const map,
           table_t **table,
           uint32_t hash)
{
    if (HASHMAP_MAX_LEN <= map->size)
    {
        return HASHCODE_NOSPACE;
    }

    hashcode_t code = HASHCODE_OK;

    switch (map->tabtype)
    {
        case HBIG:
            {
                code = table_grow_big(map, table, hash);
            }
            break;
        case HSMALL:
            {
                if (map->size <= HBIGMIN)
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
                    map->size = 0; // Just reset the size since we'll readd.
                    code = table_iterate(map, *table, (void *)map, table_iterate_readd_cb);
                    if (code)
                    {
                        map->size = (*table)->size;
                        map->tables = *table;
                        free(newtable);
                        break;
                    }
                    else
                    {
                        free(*table);
                        *table = newtable;
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
                    map->tables = tables;
                    tables[0] = (table_t *)map->tables;
                    tables[1] = (table_t *)map->tables;
                    code = table_grow(map, table, hash);
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
           int const headindex,
           int const index,
           bool * const notrust)
{
    uint8_t leap = slot->leaps[index_sub(index)];
    if (!(leap & SEARCH))
    {
        return (index + (int)(leap & LEAP)) & table_el_mask(table);
    }
    else
    {
        // Linear search through hashes.
        // Use same hash as previous for search efficiency,
        // flag that we can't compare with the hash for
        // equality test.
        *notrust = true;
        uint8_t searchhash = slot->hashes[index_sub(index)];

        int i;
        int len = table_slot_len(table);
        int slotmask = table_slot_mask(table);
        // Find the slot to start the search.
        int sindex = (index_slot(index) + ((int)(leap & LEAP))) & slotmask;

        // Iterate through every slot in the table starting at the leap point.
        for (i = 0; i < len; ++i)
        {
            slot = table_slot(map, table, sindex);
            int searchmap = slot_contains(slot, searchhash);
            while (searchmap)
            {
                int currsubindex = searchmap_next(searchmap);
                const void *key = slot_key(map, slot, currsubindex);
                uint32_t currhash = hash_fib(map->hash_cb(key));
                int currindex = table_hash_index(table, currhash);
                int finalindex = index_from(sindex, currsubindex);
                if (headindex == currindex && finalindex != index)
                {
                    return finalindex;
                }
                searchmap = searchmap_clear(searchmap, currsubindex);
            }

            sindex = (sindex + 1) & slotmask;
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
#if DEBUG
    printf("table_find_end start\n");
#endif
    for (;;)
    {
        slot_t *slot = table_slot(map, table, index_slot(index));
        if (0 == (slot->leaps[index_sub(index)] & LEAP))
        {
            return index;
        }

        bool scratch;
        index = table_leap(map, table, slot, origindex, index, &scratch);
    }
#if DEBUG
    printf("table_find_end end\n");
#endif

    // We should never get here.
    return 0;
}

/**
 * @brief Remove the index from the linked list.
 * @warn Calling with unindex set to a head is an error.
 * @warn Does NOT set entry to EMPTY.
 * @warn Does decrement map and table.
 */
static void
table_unlink(hashmap_t * const map,
             table_t * const table,
             const int headindex,
             const int previndex,
             const int unindex)
{
    // Remove entry from linked list.
    slot_t *prevslot = table_slot(map, table, index_slot(previndex));
    slot_t *unslot = table_slot(map, table, index_slot(unindex));
    uint8_t unleap = unslot->leaps[index_sub(unindex)];
    uint8_t prevleap = prevslot->leaps[index_sub(previndex)];

    if (0 == (unleap & LEAP))
    {
#if DEBUG
            printf("UNLINK TAIL\n");
#endif
        prevslot->leaps[index_sub(previndex)] = HEAD & prevleap;
    }
    else
    {
#if DEBUG
            printf("REPLACE LINK\n");
#endif

        bool badsearch = true;
        if (!(prevleap & SEARCH) && !(unleap & SEARCH))
        {
            int dist = (int)(prevleap & LEAP) + (int)(unleap & LEAP);
            if (dist < LEAPMAX)
            {
#if DEBUG
                printf("EASY RELINK\n");
#endif
                // Good, we don't have to resort to inefficient searching.
                prevslot->leaps[index_sub(previndex)] =
                    (prevleap & HEAD) | ((uint8_t)dist);
                badsearch = false;
            }
        }

        bool scrap;
        int nextindex = table_leap(map, table, unslot, headindex, unindex, &scrap);

        if (badsearch)
        {
#if DEBUG
            printf("HARD RELINK\n");
#endif
            int prevsindex = index_slot(previndex);
            int nextsindex = index_slot(nextindex);
            int dist =
                (nextsindex < prevsindex)
                ? ((nextsindex + table_slot_len(table)) - prevsindex)
                : (nextsindex - prevsindex);
#if DEBUG
            printf("head: %d\n", headindex);
            printf("prev: %d %d %d\n", previndex, prevsindex, index_sub(previndex));
            printf("next: %d %d %d\n", nextindex, nextsindex, index_sub(nextindex));
            printf("dist: %d\n", dist);
#endif

            uint8_t newleap;
            if (dist < LEAPMAX)
            {
                newleap = SEARCH | ((uint8_t)dist);
            }
            else
            {
                newleap = SEARCH | (LEAPMAX - 1);
            }

            // Propagate the subhash.
            slot_t *nextslot = table_slot(map, table, nextsindex);
            uint8_t prevsubhash = prevslot->hashes[index_sub(previndex)];
            if (SEARCH & nextslot->leaps[index_sub(nextindex)])
            {
#if DEBUG
                printf("CASCADE\n");
#endif
                int nindex = nextindex;
                slot_t *nslot = nextslot;
#if DEBUG
                printf("NextLeap: %X\n", (int)nextslot->leaps[index_sub(nextindex)]);
#endif
                for (;;)
                {
                    bool scratch;
                    int nnindex = table_leap(map, table, nslot, headindex, nindex, &scratch);

                    // We can change the next's subhash.
                    nslot->hashes[index_sub(nindex)] = prevsubhash;
                    // Advance the slot pointer.
                    nslot = table_slot(map, table, index_slot(nnindex));
                    // Get the next mask.
                    uint8_t nnleap = nslot->leaps[index_sub(nnindex)];
#if DEBUG
                    printf("NNIndex: %d\n", nnindex);
                    printf("NNextLeap: %X\n", (int)nnleap);
#endif
                    nindex = nnindex;
                    if (0 == (LEAP & nnleap))
                    {
                        // If the next entry is the end, we're done.
                        break;
                    }
                    if (!(SEARCH & nnleap))
                    {
                        // If the next entry isn't a search, we're done.
                        break;
                    }
                }

                // Change the next subhash
                nslot->hashes[index_sub(nindex)] = prevsubhash;
            }
            else
            {
#if DEBUG
                    printf("SINGLE CASCADE\n");
#endif
                    // Correct the next index's subhash.
                    nextslot->hashes[index_sub(nextindex)] = prevsubhash;
            }

            // Good, we can just replace the previous leap.
            prevslot->leaps[index_sub(previndex)] =
                (prevleap & HEAD) | newleap;
        }
    }
    --map->size;
    --table->size;
}

/**
 * @brief Find empty.
 */
static inline int
table_find_empty(hashmap_t const * const map,
                 table_t const * const table,
                 slot_t const * slot, // Starts out pointing to afterindex.
                 const int afterindex)
{
    int emptyindex = -1;

    int i;
    int len = table_slot_len(table);
    int sindex = index_slot(afterindex);
    int subindex = index_sub(afterindex);
    int searchmap = slot_contains_after(slot, EMPTY, subindex);
    int slotmask = table_slot_mask(table);

    for (i = 0; i < len; ++i)
    {
        if (searchmap)
        {
            subindex = searchmap_next(searchmap);
            emptyindex = index_from(sindex, subindex);
            break;
        }

        sindex = (sindex + 1) & slotmask;
        slot = table_slot(map, table, sindex);
        searchmap = slot_contains(slot, EMPTY);
    }

    if (i == len)
    {
        // Note that the slot and sindex are already back where we started
        // by this point.
        searchmap = searchmap_limit_before(searchmap, subindex);
        if (searchmap)
        {
            subindex = searchmap_next(searchmap);
            emptyindex = index_from(sindex, subindex);
        }
    }

    return emptyindex;
}

/**
 * @brief Find the next available slot and place the element.
 */
static hashcode_t
table_emplace(hashmap_t * const map,
              table_t *table,
              slot_t *tailslot,
              const uint32_t hash,
              const void *key,
              const void *val,
              const int headindex,
              const int tailindex)
{
    if (table_is_full(table))
    {
        hashcode_t code = table_grow(map, &table, hash);
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
#if DEBUG
        printf("EMPLACE HEAD: %d\n", headindex);
        printf("EMPLACE TAIL: %d\n", tailindex);
#endif
        slot_t *slot = table_slot(map, table, index_slot(tailindex));
        int emptyindex = table_find_empty(map, table, slot, tailindex);
        // Update slot pointer to point to empty for placement.
        slot = table_slot(map, table, index_slot(emptyindex));
        int aftertail = index_loc(headindex, tailindex, emptyindex, table_len(table), table_el_mask(table));

#if DEBUG
        printf("EMPLACE EMPTY: %d\n", emptyindex);
        printf("LEN: %X\n", table_len(table));
        printf("MASK: %X\n", table_el_mask(table));
        printf("AFTER TAIL: %d\n", aftertail);
#endif


        uint8_t newleap = 0;
        uint8_t subhash = 0;
        if (aftertail > 0)
        {
            int dist = table_index_dist(table, tailindex, emptyindex);
            uint8_t tailleap = HEAD & tailslot->leaps[index_sub(tailindex)];
            if (dist < LEAPMAX)
            {
                tailleap |= dist;
                subhash = hash_sub(hash);
            }
            else
            {
                dist = dist / SLOTLEN;
                if (dist >= LEAPMAX)
                {
                    dist = LEAPMAX - 1;
                }
                tailleap |= SEARCH | dist;
                subhash = tailslot->hashes[index_sub(tailindex)];
            }

            tailslot->leaps[index_sub(tailindex)] = tailleap;
        }
        else
        {
        }

        return table_place(map, table, slot, index_sub(emptyindex),
                           subhash, newleap, key, val);
    }
}

static inline int
table_find_prev_to_index(hashmap_t const * const map,
                         table_t const * const table,
                         slot_t const * slot,
                         int origindex,
                         int searchindex)
{
    int index = origindex;
    bool scrap;
#if DEBUG
    printf("table_find_prev_to_index start\n");
#endif
    for (;;)
    {
        slot = table_slot(map, table, index_slot(index));
        int nextindex = table_leap(map, table, slot, origindex, index, &scrap);
        if (nextindex == searchindex)
        {
            return index;
        }
        index = nextindex;
    }
#if DEBUG
    printf("table_find_prev_to_index end\n");
#endif

    return 0;
}

/**
 * @brief Relocate the given value.
 */
static hashcode_t
table_re_emplace(hashmap_t * const map,
                 table_t *table,
                 slot_t *slot,
                 const uint32_t hash,
                 const void *key,
                 const void *val,
                 int origindex)
{
    if (table_is_full(table))
    {
        hashcode_t code = table_grow(map, &table, hash);
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
        uint32_t currhash = hash_fib(map->hash_cb(currkey));
        int headindex = table_hash_index(table, currhash);
        int previndex = table_find_prev_to_index(map, table, slot, headindex, origindex);
        // Unlink.
        table_unlink(map, table, headindex, previndex, origindex);
        // Find end of list.
        int emplaceindex = table_find_end(map, table, headindex, previndex);

        // Emplace key/val.
        const void *currval = (const char *)currkey + map->keysize;
        slot_t *emplaceslot = table_slot(map, table, index_slot(emplaceindex));
        hashcode_t code = table_emplace(map, table, emplaceslot,
                                        currhash, currkey, currval,
                                        headindex, emplaceindex);

        if (code)
        {
            return code;
        }

        // Place new key/val
        return table_place(map, table, slot, index_sub(origindex), hash_sub(hash), HEAD, key, val);
    }
}

static inline void *
table_get(hashmap_t const * const map,
          table_t const * const table,
          const uint32_t hash,
          const void *key)
{
    void *val = NULL;

#if DEBUG
    printf("table_get start\n");
#endif

    do
    {
        int headindex = table_hash_index(table, hash);
        int subindex = index_sub(headindex);
        slot_t *slot = table_slot(map, table, index_slot(headindex));

        if (EMPTY == slot->hashes[subindex])
        {
            break;
        }

        if (!(slot->leaps[subindex] & HEAD))
        {
            break;
        }

        const uint8_t subhash = hash_sub(hash);
        int index = headindex;
        bool notrust = false;
        for (;;)
        {
            if ((subhash == slot->hashes[subindex]) || notrust)
            {
                // Maybe already exists.
                const void *key2 = slot_key(map, slot, subindex);
                if (map->eq_cb(key, key2))
                {
                    val = ((char *)key2) + map->keysize;
                    break;
                }
            }

            if (0 == (slot->leaps[subindex] & LEAP))
            {
                break;
            }

            notrust = false;
            index = table_leap(map, table, slot, headindex, index, &notrust);
            subindex = index_sub(index);
            slot = table_slot(map, table, index_slot(index));
        }
    }
    while (false);

#if DEBUG
    printf("table_get end\n");
#endif
    return val;
}

static inline hashcode_t 
table_insert(hashmap_t * const map,
             table_t *table,
             const uint32_t hash,
             const void *key,
             const void *val)
{
    hashcode_t code = HASHCODE_ERROR;

#if DEBUG
    printf("table_insert start\n");
    printf("Inserting: %d\n", *((const int *)key));
#endif

    do
    {
        int headindex = table_hash_index(table, hash);
        const uint8_t subhash = hash_sub(hash);
        int subindex = index_sub(headindex);
        slot_t *slot = table_slot(map, table, index_slot(headindex));

        if (EMPTY == slot->hashes[subindex])
        {
            // Empty, the optimal case.
#if DEBUG
            printf("DIRECT\n");
#endif
            code = table_place(map, table, slot, subindex, subhash, HEAD, key, val);
            break;
        }

        if (!(slot->leaps[subindex] & HEAD))
        {
            // Worst case scenario.
            // Cons of linked list, relocate.
#if DEBUG
            printf("RE-EMPLACE\n");
#endif
            code = table_re_emplace(map, table, slot, hash, key, val, headindex);
            break;
        }

#if DEBUG
    printf("HeadIndex: %d\n", headindex);
#endif

        int index = headindex;
        bool notrust = false;
        for (;;)
        {
            if ((subhash == slot->hashes[subindex]) || notrust)
            {
                // Maybe already exists.
                const void *key2 = slot_key(map, slot, subindex);
                if (map->eq_cb(key, key2))
                {
#if DEBUG
                    printf("EXISTS\n");
#endif
                    code = HASHCODE_EXIST;
                    break;
                }
            }

            if (0 == (slot->leaps[subindex] & LEAP))
            {
#if DEBUG
                printf("EMPLACE\n");
#endif
                code = table_emplace(map, table, slot, hash, key, val, headindex, index);
                break;
            }

            notrust = false;
            index = table_leap(map, table, slot, headindex, index, &notrust);
            subindex = index_sub(index);
            slot = table_slot(map, table, index_slot(index));
        }
    }
    while (false);

#if DEBUG || INVARIANT
    hashmap_invariant(map);
#endif

    // Realistically we should never get here.
    return code;
}

static inline hashcode_t
table_remove(hashmap_t * const map,
             table_t * const table,
             const uint32_t hash,
             void const * const key,
             void * kout,
             void * vout)
{
    hashcode_t code = HASHCODE_NOEXIST;

    do
    {
        int headindex = table_hash_index(table, hash);
        int subindex = index_sub(headindex);
        slot_t *slot = table_slot(map, table, index_slot(headindex));

        if (EMPTY == slot->hashes[subindex])
        {
            break;
        }

        if (!(slot->leaps[subindex] & HEAD))
        {
            break;
        }

        const uint8_t subhash = hash_sub(hash);
        int previndex = headindex;
        int index = headindex;
        bool notrust = false;
        for (;;)
        {
            if ((subhash == slot->hashes[subindex]) || notrust)
            {
                // Maybe exists.
                const void *key2 = slot_key(map, slot, subindex);
                if (map->eq_cb(key, key2))
                {
                    if (HEAD & slot->leaps[subindex])
                    {
                        if (0 != (slot->leaps[subindex] & LEAP))
                        {
                            // Unlink and copy the next item.
                            previndex = index;
                            index = table_leap(map, table, slot,
                                               headindex, index, &notrust);
                            table_unlink(map, table,
                                         headindex, previndex, index);
                            slot_export(map, slot, kout, vout, subindex);
                            table_copy_entry(map, table, index, headindex);
                            slot = table_slot(map, table, index_slot(index));
                            subindex = index_sub(index);
                            if (notrust)
                            {
                                // Update the hash.
                                slot->hashes[subindex] =
                                    hash_fib(map->hash_cb(slot_key(map, slot, subindex)));
                            }
                            // Transfer the hash.
                            slot_t *headslot = table_slot(map, table, index_slot(headindex));
                            headslot->hashes[index_sub(headindex)] = slot->hashes[subindex];
                            kout = NULL;
                            vout = NULL;
                        }
                        else
                        {
                            // Only entry, it will get cleared below.
                        }
                    }
                    else
                    {
                        table_unlink(map, table, headindex, previndex, index);
                    }

                    slot->hashes[subindex] = EMPTY;
                    slot_export(map, slot, kout, vout, subindex);

                    code = HASHCODE_OK;
                    break;
                }
            }

            if (0 == (slot->leaps[subindex] & LEAP))
            {
                break;
            }

            notrust = false;
            previndex = index;
            index = table_leap(map, table, slot, headindex, index, &notrust);
            subindex = index_sub(index);
            slot = table_slot(map, table, index_slot(index));
        }
    }
    while (false);

    return code;
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
hashmap_get(hashmap_t const * const map,
            const void *key)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = map_tables(map)[table_choose(map, hash)];
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
hashmap_getkey(hashmap_t const * const map,
               const void *key)
{
    char *val = (char *)hashmap_get(map, key);

    if (NULL != val)
    {
        return (void *)(val - map->keysize);
    }

    return (void *)val;
}


bool
hashmap_contains(hashmap_t const * const map,
                 const void *key)
{
    return !(NULL == hashmap_get(map, key));
}

hashcode_t
hashmap_iterate(hashmap_t const * const map,
                void *ud,
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

void
hashmap_clear(hashmap_t * const map)
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

    int i;
    for (i = 0; i < map->tablen; ++i)
    {
        table_t *table = tables[i];
        table_clear(table, map->slotsize);
    }
    map->size = 0;
}

hashcode_t
hashmap_insert(hashmap_t * const map,
               const void *key,
               const void *val)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = map_tables(map)[table_choose(map, hash)];
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
hashmap_remove(hashmap_t * const map,
               void const * const key,
               void *kout,
               void *vout)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = map_tables(map)[table_choose(map, hash)];
            }
            break;
        case HSMALL:
            {
                table = (table_t *)map->tables;
            }
            break;
        case HEMPTY:
            {
                return HASHCODE_NOEXIST;
            }
            break;
    }

    return table_remove(map, table, hash, key, kout, vout);
}

/** DEBUG FUNCTIONS **/

static int
head_invariant(hashmap_t const * const map,
               table_t const * const table,
               const int headindex)
{
    int listlen = 0;
    int previndex = -1;
    int index = headindex;
    int len = table_len(table);
    bool notrust = false;

    int i;
    for (i = 0; i < len; ++i)
    {
        ++listlen;
        slot_t *slot = table_slot(map, table, index_slot(index));
        const void *key = slot_key(map, slot, index_sub(index));
        uint32_t hash = hash_fib(map->hash_cb(key));
        uint8_t subhash = hash_sub(hash);
        uint8_t leap = slot->leaps[index_sub(index)];

        int origindex = table_hash_index(table, hash);
        if (origindex != headindex)
        {
            hashmap_print(map);
            printf("Entry index wrong list: is [%d] expected [%d] at [%d]\n",
                   origindex, headindex, index);
            exit(1);
        }

        if (previndex >= 0)
        {
            int revisedindex = index < headindex ? index + len : index;
            int revisedprevindex = previndex < headindex ? previndex + len : previndex;
            if (revisedindex < revisedprevindex)
            {
                hashmap_print(map);
                printf("Invalid index progression: prev|norm [%d|%d] curr|norm [%d|%d] len [%d]\n",
                       previndex, revisedprevindex, index, revisedindex, len);
                exit(1);
            }
            else if (index == headindex)
            {
                hashmap_print(map);
                printf("Cycle, starting back at head: head[%d]\n", headindex);
                exit(1);
            }
            else if (index == previndex)
            {
                hashmap_print(map);
                printf("Cycle, index = previndex: index [%d]\n", index);
                exit(1);
            }
        }

        if (!notrust)
        {
            if (subhash != slot->hashes[index_sub(index)])
            {
                hashmap_print(map);
                printf("Subhash error: is [%X] expected [%X]\n",
                       (int)slot->hashes[index_sub(index)], (int)subhash);
                exit(1);
            }
        }

        if (0 == (leap & LEAP))
        {
            break;
        }

        previndex = index;
        index = table_leap(map, table, slot, headindex, index, &notrust);
    }

    return listlen;
}

static void
table_invariant(hashmap_t const * const map,
                table_t const * const table)
{
    int i;
    int len = table_slot_len(table);
    int maxentries = len * SLOTLEN;

    if (1 != pop_count(len))
    {
        printf("Table length not pwr2: is [%d]\n", len);
    }

    // Check table size.
    int emptycount = 0;
    for (i = 0; i < len; ++i)
    {
        slot_t *slot = table_slot(map, table, i);
        int searchmap = slot_contains(slot, EMPTY);
        emptycount += pop_count(searchmap);
    }

    int size = maxentries - emptycount;
    if (size != table->size)
    {
        hashmap_print(map);
        printf("Table size error: is [%d] expected [%d]\n", table->size, size);
        exit(1);
    }

    // Check links.
    int traversed = 0;
    for (i = 0; i < len; ++i)
    {
        slot_t *slot = table_slot(map, table, i);

        int sub;
        for (sub = 0; sub < SLOTLEN; ++sub)
        {
            uint8_t subhash = slot->hashes[sub];
            uint8_t leap = slot->leaps[sub];
            if (EMPTY != subhash)
            {
                // We have a valid entry.
                if (leap & HEAD)
                {
                    // We have a valid head of a list.
                    traversed += head_invariant(map, table, index_from(i, sub));
                }
            }
        }
    }

    if (traversed != table->size)
    {
        hashmap_print(map);
        printf("Traversed more items than table should have: is [%d] expected [%d]\n", traversed, table->size);
        exit(1);
    }
}

void
hashmap_invariant(hashmap_t const * const map)
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
                if (1 != map->tablen)
                {
                    hashmap_print(map);
                    printf("Invalid table length for SMALL type: is [%d] expected [1]\n", map->tablen);
                    exit(1);
                }
                tables = (table_t **)&(map->tables);
            }
            break;
        case HEMPTY:
            {
                if (0 != map->tablen)
                {
                    hashmap_print(map);
                    printf("Invalid table length for EMPTY type: is [%d] expected [0]\n", map->tablen);
                    exit(1);
                }
            }
            break;
        default:
            {
                hashmap_print(map);
                printf("Invalid table type: is [%d] expected [0,1,2]\n", map->tabtype);
                exit(1);
            }
            break;
    }

    int size = 0;

    int i;
    for (i = 0; i < map->tablen; ++i)
    {
        table_t *table = tables[i];

        // Total size checked at end.
        size += table->size;

        // Check the table.
        table_invariant(map, table);
    }

    if (map->size != size)
    {
        hashmap_print(map);
        printf("Map size fail: is [%d] expected [%d]\n", map->size, size);
        exit(1);
    }
}

#define LINE "----------------------------------\n"

static void
print_key(char const * const key,
          const int size)
{
    int i;
    for (i = 0; i < size; ++i)
    {
        printf("%02X", key[i] & 0xFF);
    }
}

static void
table_print_slots(hashmap_t const * const map,
                  table_t const * const table)
{
    int sindex;
    int slen = table_slot_len(table);
    for (sindex = 0; sindex < slen; ++sindex)
    {
        printf("SLOT: %d\n", sindex);
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
                    printf(" (link");
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

#define STATLEN (32)

/**
 * @return Any counts not within the alloted space.
 */
static int
head_count(hashmap_t const * const map,
           table_t const * const table,
           const int headindex,
           int64_t *dists,
           int *stats)
{
    int overflow = 0;
    int len = table_len(table);
    int index = headindex;

    int i;
    for (i = 0; i < len; ++i)
    {
        if (i < STATLEN)
        {
            ++stats[i];
            dists[i] += table_index_dist(table, headindex, index);
        }
        else
        {
            ++overflow;
        }

        slot_t *slot = table_slot(map, table, index_slot(index));
        uint8_t leap = slot->leaps[index_sub(index)];

        if (0 == (leap & LEAP))
        {
            break;
        }

        bool scratch;
        index = table_leap(map, table, slot, headindex, index, &scratch);
    }

    return overflow;
}

static int
table_print_stats(hashmap_t const * const map,
                  table_t const * const table,
                  int64_t *dists,
                  int *totals)
{
    int stats[STATLEN] = { 0 };
    int overflow = 0;

    int i;
    int len = table_slot_len(table);
    for (i = 0; i < len; ++i)
    {
        slot_t *slot = table_slot(map, table, i);

        int sub;
        for (sub = 0; sub < SLOTLEN; ++sub)
        {
            uint8_t subhash = slot->hashes[sub];
            uint8_t leap = slot->leaps[sub];
            if (EMPTY != subhash)
            {
                // We have a valid entry.
                if (leap & HEAD)
                {
                    // We have a valid head of a list.
                    overflow += head_count(map, table, index_from(i, sub),
                                           dists, stats);
                }
            }
        }
    }

    for (i = 0; i < STATLEN; ++i)
    {
        totals[i] += stats[i];
    }

    return overflow;
}

void
hashmap_print_stats(hashmap_t const * const map)
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

    if (map->size)
    {
        int64_t dists[STATLEN] = { 0 };
        int totals[STATLEN] = { 0 };
        int overflow = 0;

        printf("TABLE\n");
        double dsize = (double)map->size;
        int i;
        for (i = 0; i < map->tablen; ++i)
        {
            table_t *table = tables[i];
            if (table->index == i)
            {
                printf("%.03d: %.04f (%d)\n",
                       i, (double)table->size/dsize, table->size);
            }
            else
            {
                printf("%.03d: duplicate of %d\n", i, table->index);
            }
        }

        for (i = 0; i < map->tablen; ++i)
        {
            table_t *table = tables[i];
            overflow += table_print_stats(map, table, dists, totals);
        }

        printf("LINKED LIST\n");
        for (i = 0; i < STATLEN; ++i)
        {
            int llindex = i;
            double percenttotal = (double)totals[i]/dsize;
            int lltotal = totals[i];
            double avgdist = 0.0;
            if (totals[i])
            {
                avgdist = (double)dists[i]/(double)totals[i];
            }
            printf("%.02d: %.04f (%d) avg dist (%.04f)\n",
                   llindex, percenttotal, lltotal, avgdist);
        }

        printf("Over %d: %.04f (%d)\n",
               STATLEN, (double)overflow/dsize, overflow);
    }
    else
    {
        printf("No stats (empty)\n");
    }
}

