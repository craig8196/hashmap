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

    // TODO remove hash callback?
    // TODO make first item of the eq_cb the key?

typedef enum hashcode_e
{
    HASHCODE_OK      = 0,
    HASHCODE_STOP    = 1,
    HASHCODE_EXIST   = 2,
    HASHCODE_NOEXIST = 3,
    HASHCODE_NOMEM   = 4,
    HASHCODE_NOSPACE = 5,
}
hashcode_t;

typedef uint32_t(*hashmap_hash_cb_t)(const void *el);
typedef bool(*hashmap_eq_cb_t)(const void *el1, const void *el2);
typedef hashcode_t(*hashmap_iterate_cb_t)(void *ud, void *el);

typedef struct hashmap_s hashmap_t;
#include "hashmap_internal.h"

// Init/Destroy
hashcode_t
hashmap_init(hashmap_t *map,
             int nslots,
             hashmap_hash_cb_t hash_cb,
             hashmap_eq_cb_t eq_cb);
void
hashmap_destroy(hashmap_t *map);

// Queries
int
hashmap_size(hashmap_t *map);
int
hashmap_capacity(hashmap_t *map);
bool
hashmap_is_empty(hashmap_t *map);
hashcode_t
hashmap_get(hashmap_t *map,
            void *el,
            void **save);
bool
hashmap_contains(hashmap_t *map,
                 void *el);
hashcode_t
hashmap_iterate(hashmap_t *map,
                void *ud,
                hashmap_iterate_cb_t iter_cb);

// Modifiers
void
hashmap_clear(hashmap_t *map);
hashcode_t
hashmap_insert(hashmap_t *map,
               void *el,
               void **upsert);
hashcode_t
hashmap_remove(hashmap_t *map,
               void *el,
               void **save);


#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_H */

