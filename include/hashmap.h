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

typedef uint32_t(*hashmap_hash_cb_t)(const void *key);
typedef bool(*hashmap_eq_cb_t)(const void *key1, const void *key2);
typedef int(*hashmap_iterate_cb_t)(void *ud, const void *key, void *el);

typedef struct hashmap_s hashmap_t;
#include "hashmap_internal.h"

// Init/Destroy
hashcode_t
hashmap_init(hashmap_t *map,
             int nslots,
             int keysize,
             int elsize,
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

void *
hashmap_get(hashmap_t *map,
            const void *key);

bool
hashmap_contains(hashmap_t *map,
                 const void *key);

hashcode_t
hashmap_iterate(hashmap_t *map,
                void *ud,
                hashmap_iterate_cb_t iter_cb);

// Modifiers
void
hashmap_clear(hashmap_t *map);

hashcode_t
hashmap_insert(hashmap_t *map,
               const void *key,
               const void *el,
               bool upsert);

hashcode_t
hashmap_remove(hashmap_t *map,
               const void *key,
               void *keyout,
               void *elout);


#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_H */

