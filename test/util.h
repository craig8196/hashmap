/**
 * @file util.h
 * @author Craig Jacobson
 * @brief Some basic testing utilities.
 */
#ifndef TESTUTIL_H
#define TESTUTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>


uint32_t
int_hash_cb(const void *key);
uint32_t
int_badhash_cb(const void *key);
bool
int_eq_cb(const void *key1, const void *key2);
int
int_cmp_cb(const void *key1, const void *key2);
void
int_sort(int * arr, const int len);

/**
 * @see http://c-faq.com/lib/randrange.html
 * @return Random int value in [low, high].
 */
int
rand_int_range(int low, int high);

/**
 * @return New int array of length \len.
 */
int *
rand_intarr_new(const int len, int *seedout, int forceseed);
void
rand_intarr_free(int *arr);

#ifdef __cplusplus
}
#endif
#endif /* TESTUTIL_H */

