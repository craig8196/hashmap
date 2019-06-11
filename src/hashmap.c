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
static const int HASHMAP_MAX_LEN = 1 << 30;
#endif

/**
 * Limit the maximum number of tables to 256.
 */
static const int HASHMAP_MAX_TABLE_LEN = 1 << 8;

/**
 * Magic number to indicate empty cell.
 */
static const uint8_t EMPTY = 0xFF;
/**
 * Subhash value that cannot be searched for.
 */
static const uint8_t UNSEARCHABLE = 0x80;
static const uint8_t HEAD = 0x80;
//static const uint8_t MASK = 0xC0;
static const uint8_t LEAP = 0x3F;
static const uint8_t SEARCH = 0x40;

/**
 * Max distance before we switch to block searching.
 * Minimum for this map to work is 15.
 */
//static const int LEAPMAX = 1 << 4;
static const int LEAPMAX = 1 << 6;

/**
 * Minimum BIG table allocation.
 */
static const int HBIGMIN = 1 << 13;
/**
 * Minimum BIG table allocation when just needing to fill space.
 */
//static const int HBIGFILL = 1 << 9;

/**
 * Number of elements per slot.
 */
#define SLOTLEN (16)
static const int SLOTSEARCH = 0x0000FFFF;

/******************************************************************************
 * STRUCTS
 ******************************************************************************/

typedef enum hashtype_e
{
    HBIG   = 0,
    HSMALL = 1,
    HEMPTY = 2,
}
hashtype_t;

typedef struct slot_s
{
    uint8_t       hashes[SLOTLEN];
    uint8_t       leaps[SLOTLEN];
    // char         entries[];
}
slot_t;

typedef struct table_s
{
    int           size;
    int           len;
    int           load;
    int           elmask;
    int           slotmask;
    int           index;
    //char          slots[];
}
table_t;

/******************************************************************************
 * UTIL FUNCTIONS
 ******************************************************************************/

/**
 * @return Number of bits set.
 */
static inline int
pop_count(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

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
            *((uint8_t *)to) = *((const uint8_t *)from);
            break;
        case sizeof(uint16_t):
            *((uint16_t *)to) = *((const uint16_t *)from);
            break;
        case sizeof(uint32_t):
            *((uint32_t *)to) = *((const uint32_t *)from);
            break;
        case sizeof(uint64_t):
            *((uint64_t *)to) = *((const uint64_t *)from);
            break;
        default:
            memcpy(to, from, size);
            break;
    }
}

/******************************************************************************
 * HASH FUNCTIONS
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
}

/**
 * @return Load factor.
 */
static int
load_factor_cb(int maxlen)
{
    return ((maxlen / SLOTLEN) * (SLOTLEN - 1));
}

/******************************************************************************
 * INDEX FUNCTIONS
 ******************************************************************************/

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
index_from(int sindex, int subindex)
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
 * @return Positive if test is after end; negative if test is after start.
 */
static inline int
index_loc(int start, int end, int test, int len, int mask)
{
    // I think this works properly.
    // Len is power of two and mask is the mask for modulo.
    // I'm not sure this works under subtraction...
    // (A + B) mod M = ((A mod M) + (B mod M)) mod M
    // Find tail delta from head: (tail + length) - head modulo len
    // Find empty delta from head: (empty + length) - head modulo len
    // Return difference of empty and tail deltas.
    return (((test + len) - start) & mask)
           - (((end + len) - start) & mask);
}

/**
 * @return Number of entries between, including last index.
 */
static inline int
index_dist(int index1, int index2, int len)
{
    return ((index2 < index1) ? (index2 + len) - index1 : index2 - index1);
}

/******************************************************************************
 * LEAP FUNCTIONS
 ******************************************************************************/

static inline bool
leap_local(uint8_t leap)
{
    return !(leap & SEARCH);
}

static inline bool
leap_end(uint8_t leap)
{
    return 0 == (leap & LEAP);
}

/******************************************************************************
 * SEARCHMAP FUNCTIONS
 ******************************************************************************/

/**
 * @return Integer value with bits set < subindex.
 */
static inline int
searchmap_limit_before(int searchmap, int subindex)
{
    return (searchmap & ((1 << subindex) - 1));
}

/**
 * @return Integer value with bits set >= subindex.
 */
static inline int
searchmap_limit_after(int searchmap, int subindex)
{
    return (searchmap & ~((1 << subindex) - 1));
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

/******************************************************************************
 * SLOT FUNCTIONS
 ******************************************************************************/

/**
 * @return Searchmap, an integer value with bits set to matching entries.
 */
static inline int
slot_find(const slot_t * slot, const uint8_t searchhash)
{
#if defined __SSE2__
    // Documentation:
    // https://software.intel.com/sites/landingpage/IntrinsicsGuide/
    __m128i first = _mm_set1_epi8((char)searchhash);
    __m128i second = _mm_loadu_si128((__m128i *)(slot->hashes));
    return _mm_movemask_epi8(_mm_cmpeq_epi8(first, second));
#else
    // Slower implementation available if we don't have SSE instructions.
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

static inline bool
slot_is_empty(const slot_t *slot, int sub)
{
    return (EMPTY == slot->hashes[sub]);
}

static inline bool
slot_is_head(const slot_t *slot, int sub)
{
    return (HEAD & slot->leaps[sub]);
}

static inline bool
slot_is_link(const slot_t *slot, int sub)
{
    return !slot_is_head(slot, sub);
}

static inline bool
slot_is_end(const slot_t *slot, int sub)
{
    return (0 == (LEAP & slot->leaps[sub]));
}

/**
 * @return Searchmap for empty entries.
 */
static inline int
slot_find_empty(const slot_t *slot)
{
    return slot_find(slot, EMPTY);
}

/**
 * @return Searchmap for nonempty entries.
 */
static inline int
slot_find_nonempty(const slot_t *slot)
{
    return (SLOTSEARCH & (~slot_find_empty(slot)));
}

/******************************************************************************
 * SIMPLE TABLE FUNCTIONS
 ******************************************************************************/

/**
 * @return Cast of slots.
 */
static inline char *
table_slots(const table_t *table)
{
    return ((char *)table + sizeof(table_t));
}

/**
 * @return Element mask.
 */
static inline int
table_mask(const table_t *table)
{
    return (table->elmask);
}

/**
 * @return Slot mask.
 */
static inline int
table_slot_mask(const table_t *table)
{
    return (table->slotmask);
}

/**
 * @return Number of entries in the table.
 */
static inline int
table_len(const table_t *table)
{
    return (table->len);
}

/**
 * @return The number of elements allocated for.
 */
static inline int
table_slot_len(const table_t *table)
{
    return (table->len / SLOTLEN);
}

/**
 * @return True if table is considered full.
 */
static inline bool
table_is_full(const table_t *table)
{
    return (table->size >= table->load);
}

/**
 * @return Index into table from hash.
 */
static inline int
table_index(const table_t *table, uint32_t hash)
{
    return (hash & table_mask(table));
}

/******************************************************************************
 * MAP FUNCTIONS
 ******************************************************************************/

/**
 * @brief Convenience function for creating a subhash.
 * @return Subhash of existing entry.
 */
static inline uint32_t
map_hash(const hashmap_t *map, const void *key)
{
    return hash_fib(map->hash_cb(key));
}

static inline uint8_t
map_subhash(const hashmap_t *map, const void *key)
{
    return hash_sub(hash_fib(map->hash_cb(key)));
}

/** @return Pointer to the key in the slot. */
static inline const void *
map_key(const hashmap_t *map, const slot_t *slot, int subindex)
{
    return ((char *)slot + sizeof(slot_t) + (map->elsize * subindex));
}

/**
 * @return Pointer to the val in the slot.
 */
static inline const void *
map_val(const hashmap_t *map, const slot_t *slot, int subindex)
{
    char *key = (char *)map_key(map, slot, subindex);
    return (key + map->keysize);
}

/**
 * @brief Perform the necessary cast.
 */
static inline table_t **
map_tables(const hashmap_t *map)
{
    return (table_t **)map->tables;
}

/**
 * @return Index to table to choose to insert into.
 */
static inline int
map_choose(const hashmap_t *map, uint32_t hash)
{
    return ((hash >> 24) & map->tabmask);
}

static inline void
map_inc(hashmap_t *map, table_t *table)
{
    ++map->size;
    ++table->size;
}

static inline void
map_dec(hashmap_t *map, table_t *table)
{
    --map->size;
    --table->size;
}

/**
 * @brief Copy out the data.
 */
static inline void
map_export(hashmap_t *map, slot_t *slot, void *kout, void *vout, int subindex)
{
    if (kout)
    {
        const void *key = map_key(map, slot, subindex);
        copydata(kout, key, map->keysize);
    }

    if (vout)
    {
        const void *val = map_val(map, slot, subindex);
        copydata(vout, val, map->valsize);
    }
}

/**
 * @brief Place the value into the table.
 * @return HASHCODE_OK.
 */
static inline void
map_place(const hashmap_t *map, slot_t *slot, int subindex,
           uint8_t subhash, uint8_t leap,
           const void *key, const void *val)
{
    slot->hashes[subindex] = subhash;
    slot->leaps[subindex] = leap;
    char *el = (char *)map_key(map, slot, subindex);
    copydata(el, key, map->keysize);
    copydata((char *)el + map->keysize, val, map->valsize);
}

/**
 * @return Get the slot at the index.
 */
static inline slot_t *
map_slot(const hashmap_t *map, const table_t *table, int sindex)
{
    return (slot_t *)(table_slots(table) + (map->slotsize * sindex));
}

/**
 * @brief Copy the entry from one point in the table to another.
 */
static inline void
map_copy_entry(const hashmap_t *map, table_t *table, int ifrom, int ito)
{
    slot_t *fslot = map_slot(map, table, index_slot(ifrom));
    slot_t *tslot = map_slot(map, table, index_slot(ito));
#if 1
    const void *f = map_key(map, fslot, index_sub(ifrom));
    void *t = (void *)map_key(map, tslot, index_sub(ito));
    copydata(t, f, map->elsize);
#else
    const char *f = (const char *)map_key(map, fslot, index_sub(ifrom));
    char *t = (char *)map_key(map, tslot, index_sub(ito));
    copydata(t, f, map->keysize);
    copydata(t + map->keysize, f + map->keysize, map->valsize);
#endif
}

/******************************************************************************
 * TABLE FUNCTIONS
 ******************************************************************************/

static inline void
table_clear(table_t *table, int slotsize);

/**
 * @param nslots - Number of slots, must be power of 2.
 * @param slotsize - Size of one slot.
 */
static inline table_t *
table_new(const hashmap_t *map, int index, int nslots)
{
    table_t *table = (table_t *)malloc(sizeof(table_t) + (nslots * map->slotsize));
    
    if (NULL != table)
    {
        table->len = nslots * SLOTLEN;
        table->load = map->load_cb(table->len);
        if (table->load > table->len)
        {
            table->load = table->len;
        }
        if (table->load < 0)
        {
            table->load = table->len / 2;
        }
        table->elmask = (nslots * SLOTLEN) - 1;
        table->slotmask = nslots - 1;
        table->index = index;
        table_clear(table, map->slotsize);
    }

    return table;
}

static inline void
table_free(table_t *table)
{
    free(table);
}

/**
 * @brief Clear all entries in the table.
 */
static inline void
table_clear(table_t *table, int slotsize)
{
    table->size = 0;
    int i;
    slot_t *m = (slot_t *)table_slots(table);
    int slen = table_slot_len(table);
    for (i = 0; i < slen; ++i)
    {
#if 0
        memset(m->hashes, EMPTY, sizeof(m->hashes));
#else
        uint64_t *hashes = (uint64_t *)m->hashes;
        hashes[0] = 0xFFFFFFFFFFFFFFFF;
        hashes[1] = 0xFFFFFFFFFFFFFFFF;
#endif
        m = (slot_t *)((char *)m + slotsize);
    }
}

/**
 * @brief Link previous index to given index.
 * @warn Does NOT cascade hashes.
 */
static inline uint8_t
table_link(table_t *table, slot_t *slotprev, int iprev, int ilink, uint8_t subhash)
{
    int dist = index_dist(iprev, ilink, table_len(table));
    uint8_t newleap = HEAD & slotprev->leaps[index_sub(iprev)];

    if (dist < LEAPMAX)
    {
        newleap |= dist;
    }
    else
    {
        dist = dist / SLOTLEN;
        if (dist >= LEAPMAX)
        {
            dist = LEAPMAX - 1;
        }
        newleap |= SEARCH | dist;
        subhash = slotprev->hashes[index_sub(iprev)];
    }

    slotprev->leaps[index_sub(iprev)] = newleap;

    return subhash;
}

/******************************************************************************
 * COMPLEX MAP FUNCTIONS
 ******************************************************************************/

static inline hashcode_t 
map_insert(hashmap_t *map, table_t *table,
           uint32_t hash, const void *key, const void *val);

/** @brief Iterate over the table and readd to the map. */
static inline hashcode_t
map_grow_iter(hashmap_t *map, table_t *table)
{
    int sindex;
    int slen = table_slot_len(table);
    for (sindex = 0; sindex < slen; ++sindex)
    {
        slot_t *slot = map_slot(map, table, sindex);
        int i;
        for (i = 0; i < SLOTLEN; ++i)
        {
            if (EMPTY != slot->hashes[i])
            {
                const void *key = map_key(map, slot, i);
                void *val = ((char *)key) + map->keysize;
                hashcode_t code = hashmap_insert(map, key, val);
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
map_grow_split(hashmap_t * map, table_t *table, int origmapsize, int index,
               int peer, int nslots_index, int nslots_peer)
{
    table_t **tables = map_tables(map);

    table_t *indextable = table_new(map, index, nslots_index);
    if (NULL == indextable)
    {
        return HASHCODE_NOMEM;
    }

    table_t *peertable = table_new(map, peer, nslots_peer);
    if (NULL == peertable)
    {
        free(indextable);
        return HASHCODE_NOMEM;
    }

    tables[index] = indextable;
    tables[peer] = peertable;

    map->size -= table->size;
    hashcode_t code = map_grow_iter(map, table);
    map->size = origmapsize;
    if (code)
    {
        tables[index] = table;
        tables[peer] = table;
        free(peertable);
        free(indextable);
    }
    else
    {
        free(table);
    }

    return code;
}

static hashcode_t
map_grow_one(hashmap_t *map, table_t **table, int origmapsize)
{
    int slen = table_slot_len(*table) * 2;
    if (slen <= 0)
    {
        return HASHCODE_NOSPACE;
    }

    table_t *oldtable = *table;
    table_t *newtable = table_new(map, oldtable->index, slen);
    if (NULL == newtable)
    {
        return HASHCODE_NOMEM;
    }

    *table = newtable;
    map->size -= oldtable->size;
    hashcode_t code = map_grow_iter(map, oldtable);
    map->size = origmapsize;

    if (code)
    {
        *table = oldtable;
        free(newtable);
    }
    else
    {
        *table = newtable;
        free(oldtable);
    }

    return code;
}

static bool
map_grow_should_increase()
{
    return false;
}

static hashcode_t
map_grow_big(hashmap_t *map, table_t **table, uint32_t hash)
{
    hashcode_t code = HASHCODE_OK;

    int origmapsize = map->size;
    int index = (*table)->index;
    int peer = index_peer(index, map->tablen);
    table_t **tables = map_tables(map);

    if (tables[index] == tables[peer])
    {
        // Split current table if it is splitable.
        int nslots = table_slot_len(*table);
        code = map_grow_split(map, *table, origmapsize, index, peer, nslots, nslots);
        if (!code)
        {
            *table = tables[map_choose(map, hash)];
        }
    }
    // I need to develop a good heuristic for this case.
    else if (HASHMAP_MAX_TABLE_LEN > map->tablen
             && map_grow_should_increase())
    {
    }
    else
    {
        // Increase the size of the current table.
        code = map_grow_one(map, &tables[index], origmapsize);
        if (!code)
        {
            *table = tables[index];
        }
    }

    return code;
}

static hashcode_t
map_grow(hashmap_t *map, table_t **table, uint32_t hash)
{
    if (HASHMAP_MAX_LEN <= map->size)
    {
        return HASHCODE_NOSPACE;
    }

    switch (map->tabtype)
    {
        case HBIG:
            {
                return map_grow_big(map, table, hash);
            }
            break;
        case HSMALL:
            {
                if (map->size <= HBIGMIN)
                {
                    // Just regrow this table.
                    table_t **tableref = (table_t **)(&map->tables);
                    hashcode_t code = map_grow_one(map, tableref, map->size);
                    if (!code)
                    {
                        *table = (table_t *)map->tables;
                    }
                    return code;
                }
                else
                {
                    // Upgrade to big hashmap.
                    table_t **tables = (table_t **)malloc(sizeof(table_t *) * 2);
                    if (NULL == tables)
                    {
                        return HASHCODE_NOMEM;
                    }
                    map->tabtype = HBIG;
                    map->tablen = 2;
                    map->tabmask = map->tablen - 1;
                    map->tables = tables;
                    tables[0] = (table_t *)map->tables;
                    tables[1] = (table_t *)map->tables;
                    return map_grow(map, table, hash);
                }
            }
            break;
        case HEMPTY:
            {
                table_t *newtable = table_new(map, 0, 2);
                if (NULL == newtable)
                {
                    return HASHCODE_NOMEM;
                }
                map->tabtype = HSMALL;
                map->tablen = 1;
                map->tabmask = 0;
                map->tables = newtable;
                *table = newtable;
                return HASHCODE_OK;
            }
            break;
    }

    return HASHCODE_ERROR;
}

/**
 * @note Broken out into seperate function in attempt to inline leaps.
 * @return Next index.
 */
static int
map_leap_extended(const hashmap_t *map, const table_t *table,
                  int ihead, int ileap,
                  uint8_t searchhash, uint8_t leap, bool *notrust)
{
    // Linear search through hashes.
    // Use same subhash as leap entry for search efficiency.

    // Set flag that you cannot compare subhash as quick lint check.
    *notrust = true;

    int i;
    // Find the slot to start the search.
    int islot = (index_slot(ileap) + ((int)(leap & LEAP))) & table_slot_mask(table);
    // Iterate through every slot in the table starting at the leap point.
    for (i = 0; i < table_slot_len(table); ++i)
    {
        slot_t *searchslot = map_slot(map, table, islot);
        int searchmap = slot_find(searchslot, searchhash);
        // For each entry in the searchmap.
        while (searchmap)
        {
            // Discover the index that we're jumping to and test it
            // to see if it is part of the same linked list.
            int subindex = searchmap_next(searchmap);
            const void *key = map_key(map, searchslot, subindex);
            uint32_t hash = map_hash(map, key);
            int iheadlanding = table_index(table, hash);
#if 0
            // The ilanding check shouldn't be needed.
            int ilanding = index_from(islot, subindex);
            if (ihead == iheadlanding && ilanding != ileap)
            {
#else
            if (ihead == iheadlanding)
            {
                int ilanding = index_from(islot, subindex);
#endif
                return ilanding;
            }
            searchmap = searchmap_clear(searchmap, subindex);
        }

        islot = (islot + 1) & table_slot_mask(table);
    }

    // We should never get here.
    return 0;
}

/**
 * @return Index to the next value in the chain.
 */
static inline int
map_leap(const hashmap_t *map, const table_t *table,
         int ihead, int ileap, bool *notrust)
{
    slot_t *slotleap = map_slot(map, table, index_slot(ileap));
    uint8_t leap = slotleap->leaps[index_sub(ileap)];
    if (leap_local(leap))
    {
        return (ileap + (int)(leap & LEAP)) & table_mask(table);
    }
    else
    {
        uint8_t searchhash = slotleap->hashes[index_sub(ileap)];
        return map_leap_extended(map, table, ihead, ileap,
                                 searchhash, leap, notrust);
    }
}

/**
 * @return Index of the last element in the list.
 */
static inline int
map_find_end(const hashmap_t *map, const table_t *table, int ihead, int ileap)
{
    for (;;)
    {
        slot_t *slot = map_slot(map, table, index_slot(ileap));
        if (leap_end(slot->leaps[index_sub(ileap)]))
        {
            return ileap;
        }

        // Advance to the next node.
        bool scratch;
        ileap = map_leap(map, table, ihead, ileap, &scratch);
    }

    return 0;
}

static void
map_cascade(const hashmap_t *map, table_t *table,
            int ihead, int inext, uint8_t newsubhash)
{
    slot_t *slotnext = map_slot(map, table, index_slot(inext));
    for (;;)
    {
        // Get the leap.
        uint8_t leap = slotnext->leaps[index_sub(inext)];

        if (leap_end(leap))
        {
            // If the next entry is the end, we're done.
            break;
        }
        if (leap_local(leap))
        {
            // If the next entry isn't a search, we're done.
            break;
        }
        
        // Perform the search BEFORE we change the search hash.
        bool scratch;
        uint8_t subhash = slotnext->hashes[index_sub(inext)];
        int inextnext = map_leap_extended(map, table, ihead, inext,
                                          subhash, leap, &scratch);
        // We can change the next's subhash.
        slotnext->hashes[index_sub(inext)] = newsubhash;

        // Advance the slot pointer.
        inext = inextnext;
        slotnext = map_slot(map, table, index_slot(inextnext));
    }

    // We can change the next's subhash.
    slotnext->hashes[index_sub(inext)] = newsubhash;
}

/**
 * @brief Remove the index from the linked list.
 * @warn Does NOT unlink HEAD.
 * @warn Does NOT set entry to EMPTY.
 * @warn Does NOT decrement map or table.
 */
static void
map_unlink(const hashmap_t * map, table_t *table,
           int ihead, int iprev, int iunlink)
{
    // Remove entry from linked list.
    slot_t *slotprev = map_slot(map, table, index_slot(iprev));
    slot_t *slotunlink = map_slot(map, table, index_slot(iunlink));
    uint8_t leapunlink = slotunlink->leaps[index_sub(iunlink)];
    uint8_t leapprev = slotprev->leaps[index_sub(iprev)];

    if (leap_end(leapunlink))
    {
        slotprev->leaps[index_sub(iprev)] = HEAD & leapprev;
    }
    else
    {
        // If the new leap stays local then we have an efficient solution.
        if (leap_local(leapprev) && leap_local(leapunlink))
        {
            int dist = (int)(leapprev & LEAP) + (int)(leapunlink & LEAP);
            if (dist < LEAPMAX)
            {
                slotprev->leaps[index_sub(iprev)] =
                    (leapprev & HEAD) | ((uint8_t)dist);
                return;
            }
        }

        // Calculate the distance from prev to next.
        bool scrap;
        int inext = map_leap(map, table, ihead, iunlink, &scrap);

        int islotprev = index_slot(iprev);
        int islotnext = index_slot(inext);

        int dist = (islotnext < islotprev)
                   ? ((islotnext + table_slot_len(table)) - islotprev)
                   : (islotnext - islotprev);

        // Create the new leap for prev.
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
        uint8_t subhashprev = slotprev->hashes[index_sub(iprev)];
        map_cascade(map, table, ihead, inext, subhashprev);

        // Good, we can just replace the previous leap.
        slotprev->leaps[index_sub(iprev)] = (leapprev & HEAD) | newleap;
    }
}

/**
 * @brief Find empty. According to our invariant there must be one available.
 */
static inline int
map_find_empty(const hashmap_t *map, const table_t *table,
               slot_t *slottail, int itail)
{
    int isub = index_sub(itail);
    int searchmap = slot_find_empty(slottail);
    searchmap = searchmap_limit_after(searchmap, isub);

    int i;
    int islot = index_slot(itail);
    for (i = 0; i < table_slot_len(table); ++i)
    {
        if (searchmap)
        {
            isub = searchmap_next(searchmap);
            return index_from(islot, isub);
        }

        islot = (islot + 1) & table_slot_mask(table);
        slot_t *slot = map_slot(map, table, islot);
        searchmap = slot_find_empty(slot);
    }

    // Note that the slot and sindex are already back where we started
    // by this point.
    searchmap = searchmap_limit_before(searchmap, isub);
    isub = searchmap_next(searchmap);
    return index_from(islot, isub);
}

/** @note Checks for table full MUST be done BEFORE calling. */
static inline void
map_place_end(hashmap_t * map, table_t *table, int ihead, int itail,
              uint8_t subhash, const void *key, const void *val)
{
    // Find an empty slot.
    slot_t *slottail = map_slot(map, table, index_slot(itail));
    int iempty = map_find_empty(map, table, slottail, itail);
    // Update slot pointer to point to empty for placement.
    int locempty = index_loc(ihead, itail, iempty,
                             table_len(table), table_mask(table));

    uint8_t newleap = 0;
    if (locempty > 0)
    {
        // Location of empty is after the tail index.
        slot_t *slotempty = map_slot(map, table, index_slot(iempty));
        subhash = table_link(table, slottail, itail, iempty, subhash);
        map_place(map, slotempty, index_sub(iempty),
                  subhash, newleap, key, val);
    }
    else
    {
        // Location of empty between head and tail.
        int iprev = ihead;
        bool scrap;
        int inext = map_leap(map, table, ihead, ihead, &scrap);

        for (;;) 
        {
            int locindex = index_loc(ihead, inext, iempty,
                                     table_len(table), table_mask(table));
            if (locindex < 0)
            {
                // Found the indices the empty lies between.
                break;
            }
            iprev = inext;
            inext = map_leap(map, table, ihead, inext, &scrap);
        }

        slot_t *slotprev = map_slot(map, table, index_slot(iprev));
        slot_t *slotempty = map_slot(map, table, index_slot(iempty));
        slot_t *slotnext = map_slot(map, table, index_slot(inext));

        // Link previous to empty slot.
        uint8_t subhashempty =
            table_link(table, slotprev, iprev, iempty, subhash);
        // Set the empty slot.
        map_place(map, slotempty, index_sub(iempty),
                  subhashempty, newleap, key, val);
        // Generate hash of next and link empty to next.
        const void *keynext = map_key(map, slotnext, index_sub(inext));
        uint8_t subhashnext = map_subhash(map, keynext);
        subhashnext = table_link(table, slotempty, iempty, inext, subhashnext);
        // Check if we need to cascade hashes.
        if (!leap_local(slotnext->leaps[index_sub(inext)]))
        {
            bool scratch;
            int inextnext = map_leap(map, table, ihead, inext, &scratch);
            map_cascade(map, table, ihead, inextnext, subhashnext);
        }
        // Set possibly new subhash.
        // Remember prev is set to the next the subhash correspondes to.
        slotnext->hashes[index_sub(inext)] = subhashnext;
    }
}

/**
 * @brief Find the next available slot and place the element.
 */
static hashcode_t
map_emplace(hashmap_t * map, table_t *table, int ihead, int itail,
            uint32_t hash, uint8_t subhash, const void *key, const void *val)
{
    if (table_is_full(table))
    {
        hashcode_t code = map_grow(map, &table, hash);
        if (code)
        {
            return code;
        }
        else
        {
            return map_insert(map, table, hash, key, val);
        }
    }
    else
    {
        map_place_end(map, table, ihead, itail, subhash, key, val);
        map_inc(map, table);
        return HASHCODE_OK;
    }
}

/**
 * @return Index of item previous to given index.
 */
static inline int
map_find_prev(const hashmap_t *map, const table_t *table, int ihead, int ifind)
{
    int index = ihead;
    for (;;)
    {
        bool scrap;
        int inext = map_leap(map, table, ihead, index, &scrap);
        if (inext == ifind)
        {
            return index;
        }
        index = inext;
    }

    return 0;
}

/**
 * @brief Relocate the given value at new value's head.
 */
static hashcode_t
map_re_emplace(hashmap_t *map, table_t *table, slot_t *slothead, int ihead,
               uint32_t hash, uint8_t subhash, const void *key, const void *val)
{
    if (table_is_full(table))
    {
        hashcode_t code = map_grow(map, &table, hash);
        if (code)
        {
            return code;
        }
        else
        {
            return map_insert(map, table, hash, key, val);
        }
    }
    else
    {
        // Get information about the item we need to replace.
        const void *currkey = map_key(map, slothead, index_sub(ihead));
        uint32_t currhash = hash_fib(map->hash_cb(currkey));
        uint32_t currsubhash = hash_sub(currhash);
        int icurrhead = table_index(table, currhash);
        // Find the entry pointing to current value.
        int icurrprev = map_find_prev(map, table, icurrhead, ihead);
        // Unlink.
        map_unlink(map, table, icurrhead, icurrprev, ihead);
        slothead->hashes[index_sub(ihead)] = UNSEARCHABLE;
        // Find end of list.
        int icurrtail = map_find_end(map, table, icurrhead, icurrprev);
        // Emplace key/val.
        const void *currval = (const char *)currkey + map->keysize;
        map_place_end(map, table, icurrhead, icurrtail,
                      currsubhash, currkey, currval);
        // Place new key/val
        map_place(map, slothead, index_sub(ihead), subhash, HEAD, key, val);
        map_inc(map, table);
        return HASHCODE_OK;
    }
}

static inline void *
map_get(const hashmap_t *map, table_t *table, const void *key, uint32_t hash)
{
    int ihead = table_index(table, hash);
    slot_t *slothead = map_slot(map, table, index_slot(ihead));

    if (slot_is_empty(slothead, index_sub(ihead)))
    {
        return NULL;
    }

    if (slot_is_link(slothead, index_sub(ihead)))
    {
        return NULL;
    }

    uint8_t subhash = hash_sub(hash);
    int index = ihead;
    slot_t *slot = slothead;
    bool notrust = false;
    for (;;)
    {
        if ((subhash == slot->hashes[index_sub(index)]) || notrust)
        {
            // Maybe already exists.
            const void *key2 = map_key(map, slot, index_sub(index));
            if (map->eq_cb(key, key2))
            {
                return ((char *)key2) + map->keysize;
            }
        }

        if (slot_is_end(slot, index_sub(index)))
        {
            return NULL;
        }

        notrust = false;
        index = map_leap(map, table, ihead, index, &notrust);
        slot = map_slot(map, table, index_slot(index));
    }

    return NULL;
}

static inline hashcode_t 
map_insert(hashmap_t *map, table_t *table,
           uint32_t hash, const void *key, const void *val)
{
    int ihead = table_index(table, hash);
    slot_t *slothead = map_slot(map, table, index_slot(ihead));
    uint8_t subhash = hash_sub(hash);

    if (slot_is_empty(slothead, index_sub(ihead)))
    {
        // Empty, the optimal case.
        map_place(map, slothead, index_sub(ihead),  subhash, HEAD, key, val);
        map_inc(map, table);
        return HASHCODE_OK;
    }

    if (slot_is_link(slothead, index_sub(ihead)))
    {
        // Worst case scenario.
        // Middle of linked list, relocate.
        return map_re_emplace(map, table, slothead, ihead, hash, subhash, key, val);
    }

    int index = ihead;
    slot_t *slot = slothead;
    bool notrust = false;
    for (;;)
    {
        if ((subhash == slot->hashes[index_sub(index)]) || notrust)
        {
            // Maybe already exists.
            const void *key2 = map_key(map, slot, index_sub(index));
            if (map->eq_cb(key, key2))
            {
                return HASHCODE_EXIST;
            }
        }

        if (slot_is_end(slot, index_sub(index)))
        {
            return map_emplace(map, table, ihead, index,
                               hash, subhash, key, val);
        }

        notrust = false;
        index = map_leap(map, table, ihead, index, &notrust);
        slot = map_slot(map, table, index_slot(index));
    }

    return HASHCODE_ERROR;
}

/**
 * @brief Remove the head of this list.
 * @warn Does not save the value at HEAD.
 */
static inline void
map_remove_head(hashmap_t *map, table_t *table, slot_t *slothead, int ihead)
{
    if (slot_is_end(slothead, index_sub(ihead)))
    {
        // Only entry.
        slothead->hashes[index_sub(ihead)] = EMPTY;
    }
    else
    {
        // Find the next index.
        bool notrust = false;
        int imove = map_leap(map, table, ihead, ihead, &notrust);
        // Unlink the item we need to move.
        map_unlink(map, table, ihead, ihead, imove);
        // Copy entry to the head.
        map_copy_entry(map, table, imove, ihead);

        // Currently, the linked list assumes that the current head is
        // the same after unlinking, but we need to update any inherited
        // subhashes if there is an extended search.

        slot_t *slotmove = map_slot(map, table, index_slot(imove));
        if (slot_is_end(slotmove, index_sub(imove)))
        {
            // Transfer the hash
            if (notrust)
            {
                const void *keyhead = map_key(map, slothead, index_sub(ihead));
                slothead->hashes[index_sub(ihead)] = map_subhash(map, keyhead);
            }
            else
            {
                slothead->hashes[index_sub(ihead)] =
                    slotmove->hashes[index_sub(imove)];
            }
            slotmove->hashes[index_sub(imove)] = EMPTY;
        }
        else
        {
            uint8_t oldsubhash = slotmove->hashes[index_sub(imove)];
            slotmove->hashes[index_sub(imove)] = EMPTY;
            notrust = false;
            int inext = map_leap(map, table, ihead, ihead, &notrust);
            if (notrust)
            {
                const void *keyhead = map_key(map, slothead, index_sub(ihead));
                uint8_t newsubhash = map_subhash(map, keyhead);
                map_cascade(map, table, ihead, inext, newsubhash);
                slothead->hashes[index_sub(ihead)] = newsubhash;
            }
            else
            {
                // Transfer the hash since it is safe.
                slothead->hashes[index_sub(ihead)] = oldsubhash;
            }
        }
    }
}

static inline hashcode_t
map_remove(hashmap_t *map, table_t *table,
           uint32_t hash, const void *key, void *kout, void *vout)
{
    int ihead = table_index(table, hash);
    slot_t *slothead = map_slot(map, table, index_slot(ihead));

    if (slot_is_empty(slothead, index_sub(ihead)))
    {
        return HASHCODE_NOEXIST;
    }

    if (slot_is_link(slothead, index_sub(ihead)))
    {
        return HASHCODE_NOEXIST;
    }

    uint8_t subhash = hash_sub(hash);
    int iprev = ihead;
    int index = ihead;
    slot_t *slot = slothead;
    bool notrust = false;
    for (;;)
    {
        if ((subhash == slot->hashes[index_sub(index)]) || notrust)
        {
            // Maybe exists.
            const void *key2 = map_key(map, slot, index_sub(index));
            if (map->eq_cb(key, key2))
            {
                // Immediately export.
                map_export(map, slot, kout, vout, index_sub(index));
                if (slot_is_head(slot, index_sub(index)))
                {
                    map_remove_head(map, table, slot, index);
                }
                else
                {
                    // Not the head, unlink and set empty.
                    map_unlink(map, table, ihead, iprev, index);
                    slot->hashes[index_sub(index)] = EMPTY;
                }
                map_dec(map, table);

                return HASHCODE_OK;
            }
        }

        if (slot_is_end(slot, index_sub(index)))
        {
            return HASHCODE_NOEXIST;
        }

        iprev = index;
        notrust = false;
        index = map_leap(map, table, ihead, index, &notrust);
        slot = map_slot(map, table, index_slot(index));
    }

    return HASHCODE_ERROR;
}

/******************************************************************************
 * BEGIN HASHMAP
 ******************************************************************************/

/** INIT/DESTROY FUNCTIONS **/

void
hashmap_init(hashmap_t * const map,
             int keysize,
             int valsize,
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb)
{
    map->size = 0;
    map->keysize = keysize;
    map->valsize = valsize;
    map->elsize = keysize + valsize;
    map->slotsize = (map->elsize * SLOTLEN) + sizeof(slot_t);
    map->tabtype = 2;
    map->tablen = 0;
    map->tabmask = 0;
    map->tables = NULL;
    map->load_cb = load_factor_cb;
    map->hash_cb = hash_cb;
    map->eq_cb = eq_cb;
}

void
hashmap_set_load_cb(hashmap_t *map, hashmap_load_cb_t load_cb)
{
    map->load_cb = load_cb;
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

        map->tables = NULL;
    }
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
hashmap_get(const hashmap_t *map, const void *key)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = map_tables(map)[map_choose(map, hash)];
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

    return map_get(map, table, key, hash);
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
            slot_t *slot = map_slot(map, table, sindex);
            int searchmap = slot_find_nonempty(slot);
            while (searchmap)
            {
                int subindex = searchmap_next(searchmap);
                const void *key = map_key(map, slot, subindex);
                void *val = ((char *)key) + map->keysize;

                hashcode_t code = iter_cb(ud, key, val);
                if (code)
                {
                    return code;
                }
                
                searchmap = searchmap_clear(searchmap, subindex);
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
hashmap_insert(hashmap_t *map, const void *key, const void *val)
{
    table_t *table = NULL;
    uint32_t hash = hash_fib(map->hash_cb(key));

    switch (map->tabtype)
    {
        case HBIG:
            {
                table = map_tables(map)[map_choose(map, hash)];
            }
            break;
        case HSMALL:
            {
                table = (table_t *)map->tables;
            }
            break;
        case HEMPTY:
            {
                hashcode_t code = map_grow(map, &table, hash);
                if (code)
                {
                    return code;
                }
            }
            break;
    }

    return map_insert(map, table, hash, key, val);
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
                table = map_tables(map)[map_choose(map, hash)];
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

    return map_remove(map, table, hash, key, kout, vout);
}

/** DEBUG FUNCTIONS **/

static hashcode_t
head_invariant(hashmap_t const * const map,
               table_t const * const table,
               const int headindex,
               int *listlenout)
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
        slot_t *slot = map_slot(map, table, index_slot(index));
        const void *key = map_key(map, slot, index_sub(index));
        uint32_t hash = hash_fib(map->hash_cb(key));
        uint8_t subhash = hash_sub(hash);
        uint8_t leap = slot->leaps[index_sub(index)];

        int origindex = table_index(table, hash);
        if (origindex != headindex)
        {
#ifdef VERBOSE
            hashmap_print(map);
#endif
            printf("Entry index wrong list: is [%d] expected [%d] at [%d]\n",
                   origindex, headindex, index);
            return HASHCODE_ERROR;
        }

        if (previndex >= 0)
        {
            int revisedindex = index < headindex ? index + len : index;
            int revisedprevindex = previndex < headindex ? previndex + len : previndex;
            if (revisedindex < revisedprevindex)
            {
#ifdef VERBOSE
                hashmap_print(map);
#endif
                printf("Invalid index progression: prev|norm [%d|%d] curr|norm [%d|%d] len [%d]\n",
                       previndex, revisedprevindex, index, revisedindex, len);
                return HASHCODE_ERROR;
            }
            else if (index == headindex)
            {
#ifdef VERBOSE
                hashmap_print(map);
#endif
                printf("Cycle, starting back at head: head[%d]\n", headindex);
                return HASHCODE_ERROR;
            }
            else if (index == previndex)
            {
#ifdef VERBOSE
                hashmap_print(map);
#endif
                printf("Cycle, index = previndex: index [%d]\n", index);
                return HASHCODE_ERROR;
            }
        }

        if (!notrust)
        {
            if (subhash != slot->hashes[index_sub(index)])
            {
#ifdef VERBOSE
                hashmap_print(map);
#endif
                printf("Subhash error: is [%X] expected [%X] at [%d]\n",
                       (int)slot->hashes[index_sub(index)], (int)subhash, index);
                fflush(stdout);
                return HASHCODE_ERROR;
            }
        }

        if (0 == (leap & LEAP))
        {
            break;
        }

        previndex = index;
        index = map_leap(map, table, headindex, index, &notrust);
    }

    *listlenout = listlen;
    return HASHCODE_OK;
}

static hashcode_t
table_invariant(hashmap_t const * const map,
                table_t const * const table)
{
    int i;
    int len = table_slot_len(table);
    int maxentries = len * SLOTLEN;

    if (1 != pop_count(len))
    {
        printf("Table length not pwr2: is [%d]\n", len);
        return HASHCODE_ERROR;
    }

    // Check table size.
    int emptycount = 0;
    for (i = 0; i < len; ++i)
    {
        slot_t *slot = map_slot(map, table, i);
        int searchmap = slot_find_empty(slot);
        emptycount += pop_count(searchmap);
    }

    int size = maxentries - emptycount;
    if (size != table->size)
    {
#ifdef VERBOSE
        hashmap_print(map);
#endif
        printf("Table size error: is [%d] expected [%d]\n", table->size, size);
        return HASHCODE_ERROR;
    }

    // Check links.
    int traversed = 0;
    for (i = 0; i < len; ++i)
    {
        slot_t *slot = map_slot(map, table, i);

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
                    int listlen = 0;
                    hashcode_t code = head_invariant(map, table, index_from(i, sub), &listlen);
                    if (code)
                    {
                        return code;
                    }
                    traversed += listlen;
                }
            }
        }
    }

    if (traversed != table->size)
    {
#ifdef VERBOSE
        hashmap_print(map);
#endif
        printf("Traversed more items than table should have: is [%d] expected [%d]\n", traversed, table->size);
        return HASHCODE_ERROR;
    }

    return HASHCODE_OK;
}

hashcode_t
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
#ifdef VERBOSE
                    hashmap_print(map);
#endif
                    printf("Invalid table length for SMALL type: is [%d] expected [1]\n", map->tablen);
                    return HASHCODE_ERROR;
                }
                tables = (table_t **)&(map->tables);
            }
            break;
        case HEMPTY:
            {
                if (0 != map->tablen)
                {
#ifdef VERBOSE
                    hashmap_print(map);
#endif
                    printf("Invalid table length for EMPTY type: is [%d] expected [0]\n", map->tablen);
                    return HASHCODE_ERROR;
                }
            }
            break;
        default:
            {
#ifdef VERBOSE
                hashmap_print(map);
#endif
                printf("Invalid table type: is [%d] expected [0,1,2]\n", map->tabtype);
                return HASHCODE_ERROR;
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
        hashcode_t code = table_invariant(map, table);
        if (code)
        {
            return code;
        }
    }

    if (map->size != size)
    {
#ifdef VERBOSE
        hashmap_print(map);
#endif
        printf("Map size fail: is [%d] expected [%d]\n", map->size, size);
        return HASHCODE_ERROR;
    }

    return HASHCODE_OK;
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
        slot_t *slot = map_slot(map, table, sindex);
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

                char const *key = (char const *)map_key(map, slot, i);
                print_key(key, map->keysize);

                uint32_t hash = map_hash(map, key);
                int myhead = table_index(table, hash);
                printf(" Head: %d Hash:%" PRIX32 " Sub:%" PRIX8, myhead, hash, hash_sub(hash));
            }
            printf("\n");
        }
    }
}

void
hashmap_print(hashmap_t const * const map)
{
    int64_t octets = sizeof(hashmap_t);
    printf("\n" LINE);
    printf("METADATA\n");
    printf(LINE);
    printf("Fibonacci: %" PRIu64 "\n", FIB);
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
    printf("Table Mask: %d\n", map->tabmask);
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
            printf("El Mask: 0x%X\n", table_mask(table));
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
    printf("Total Octets: %" PRIu64 "\n", octets);
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
            dists[i] += index_dist(headindex, index, table_len(table));
        }
        else
        {
            ++overflow;
        }

        slot_t *slot = map_slot(map, table, index_slot(index));
        uint8_t leap = slot->leaps[index_sub(index)];

        if (0 == (leap & LEAP))
        {
            break;
        }

        bool scratch;
        index = map_leap(map, table, headindex, index, &scratch);
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
        slot_t *slot = map_slot(map, table, i);

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

