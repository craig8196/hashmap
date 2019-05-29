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

/**
 * @file hashmap.h
 * @author Craig Jacobson
 * @brief Generic hash map implementation.
 */

#ifndef HASHMAP_H
#define HASHMAP_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * @brief Return codes for map functions.
 */
typedef enum hashcode_e
{
    HASHCODE_OK      = 0, /**< The operation was a success. */
    HASHCODE_STOP    = 1, /**< Iteration was stopped. */
    HASHCODE_EXIST   = 2, /**< The item already exists. */
    HASHCODE_NOEXIST = 3, /**< The item doesn't exist. */
    HASHCODE_NOMEM   = 4, /**< Not enough memory to complete operation. */
    HASHCODE_NOSPACE = 5, /**< No space in map for a new item. */
    HASHCODE_ERROR   = 6, /**< Generic error for iteration. */
}
hashcode_t;

/** @brief Hash callback prototype. */
typedef uint32_t(*hashmap_hash_cb_t)(const void *key);
/** @brief Equivalence callback prototype. */
typedef bool(*hashmap_eq_cb_t)(const void *key1, const void *key2);
/** @brief Iteration callback prototype. */
typedef hashcode_t(*hashmap_iterate_cb_t)(void *ud, const void *key, void *el);

// Don't access members directly.
typedef struct hashmap_s
{
    int                 size;
    int                 tabtype;
    int                 tablen;
    int                 tabshift;
    void               *tables;
    int                 keysize;
    int                 valsize;
    int                 elsize;
    int                 slotsize;
    hashmap_hash_cb_t   hash_cb;
    hashmap_eq_cb_t     eq_cb;
}
hashmap_t;

/**
 * @brief Create map.
 * @param keysize - Size of the value of the key. May NOT be zero.
 * @param valsize - Size of the value, may be zero.
 * @param hash_cb - Callback to determine hash.
 * @param eq_cb - Callback to determine key equality.
 * @return HASHCODE_OK on success; HASHCODE_NOMEM if no memory.
 */
hashcode_t
hashmap_init(hashmap_t *map,
             const int keysize,
             const int valsize,
             const hashmap_hash_cb_t hash_cb,
             const hashmap_eq_cb_t eq_cb);

/**
 * @brief Free used resources.
 */
void
hashmap_destroy(hashmap_t *map);

/**
 * @return Number of elements in the map.
 */
int
hashmap_size(const hashmap_t *map);

/**
 * @return True if empty; false otherwise.
 */
bool
hashmap_empty(const hashmap_t *map);

/**
 * @return Pointer to value in map, if present, valid until next insert/remove.
 */
void *
hashmap_get(const hashmap_t *map, const void *key);

/**
 * @return Pointer to key in map, if present, valid until next insert/remove.
 */
void *
hashmap_getkey(const hashmap_t *map, const void *key);

/**
 * @return True if the map contains the key.
 */
bool
hashmap_contains(const hashmap_t *map, const void *key);

/**
 * @brief Iterate over the map.
 * @return HASHCODE_OK on success; other code otherwise.
 */
hashcode_t
hashmap_iterate(const hashmap_t *map, void *ud,
                const hashmap_iterate_cb_t iter_cb);

/**
 * @brief Insert the key/val into the hashtable.
 * @return HASHCODE_OK on success;
 *         HASHCODE_NOSPACE, HASHCODE_EXIST, HASHCODE_NOMEM otherwise.
 */
hashcode_t
hashmap_insert(hashmap_t *map, const void *key, const void *val);

/**
 * @brief Remove the keyed item from the map.
 * @param key - The item to remove.
 * @param kout - Save the key stored internally.
 * @param vout - Save the value stored internally.
 * @return HASHCODE_OK on success; HASHCODE_NOEXIST if the value doesn't exist.
 */
hashcode_t
hashmap_remove(hashmap_t *map, const void *key, void *kout, void *vout);

/**
 * @brief Print useful information about the map for debugging.
 */
void
hashmap_print(const hashmap_t *map);


#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_H */

