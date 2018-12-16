
#ifndef HASHMAP_INTERNAL_H
#define HASHMAP_INTERNAL_H
#ifdef __cplusplus
extern "C" {
#endif


#include "hashmap.h"


#define HASHMAP_BUCKET_COUNT (4)
#define HASHMAP_BUCKET_LAST (HASHMAP_BUCKET_COUNT - 1)

typedef struct hashmap_slot_s hashmap_slot_t;
struct hashmap_slot_s
{
    // NOTE: Upper/negative bit in debt is used to store
    //       presence or absence of element.
    int      debt;
    uint32_t hash;
    void    *el;
};

// On the X86_64 architecture the cache line is 64 bytes.
// The hashmap_slot_t should be 16 bytes, giving us
// 4 or HASHMAP_BUCKET_COUNT slots, so four entries per cache line.
typedef struct hashmap_bucket_s hashmap_bucket_t;
struct hashmap_bucket_s
{
    hashmap_slot_t slots[HASHMAP_BUCKET_COUNT];
};

struct hashmap_s
{
    int                 size;
    int                 nslots_index;
    int                 max_hits;
    hashmap_hash_cb_t   hash_cb;
    hashmap_eq_cb_t     eq_cb;
    hashmap_bucket_t   *buckets;
};


#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_INTERNAL_H */

