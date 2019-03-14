
#ifndef HASHMAP_INTERNAL_H
#define HASHMAP_INTERNAL_H
#ifdef __cplusplus
extern "C" {
#endif


#include "hashmap.h"


struct hashmap_s
{
    int                 size;
    int                 len;
    int                 pindex;
    int                 maxrun;
    int                 keysize;
    int                 elsize;
    int                 slotsize;
    hashmap_hash_cb_t   hash_cb;
    hashmap_eq_cb_t     eq_cb;
    char                *slots;
    char                *slottmp;
    char                *slotswap;
    /*
     * The structure of the table is:
     * sizeof(uint32_t) hash
     * keysize          key
     * elsize           el
     *
     * The uppermost bit of the hash is used to indicate presence of element.
     */
};



#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_INTERNAL_H */

