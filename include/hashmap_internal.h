
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
    int                 power; // Also max run.
    int                 shift;
    int                 mask;
    int                 keysize;
    int                 elsize;
    int                 slotsize;
    hashmap_hash_cb_t   hash_cb;
    hashmap_eq_cb_t     eq_cb;
    char                *slottmp;
    char                *slotswap;
    char                *slots;
    /*
     * The structure of the table is:
     * sizeof(int)      debt
     * keysize          key
     * elsize           el
     *
     * The uppermost bit of the debt is used to indicate presence of element.
     */
};



#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_INTERNAL_H */

