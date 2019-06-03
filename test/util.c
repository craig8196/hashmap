
#include "util.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t
int_hash_cb(const void *key)
{
    if (sizeof(int) == sizeof(uint32_t))
    {
        return *((const uint32_t *)key);
    }
    else
    {
        return (uint32_t)*((const int *)key);
    }
}

uint32_t
int_badhash_cb(const void *key)
{
    key = key;
    return 1;
}

bool
int_eq_cb(const void *key1, const void *key2)
{
    return (*(int *)key1) == (*(int *)key2);
}

int
int_cmp_cb(const void *key1, const void *key2)
{
    return (*(int *)key1) - (*(int *)key2);
}

int
rand_int_range(int low, int high)
{
    int r = low + (rand() / ((RAND_MAX / (high - low + 1)) + 1));
    assert((low <= r && r <= high) && "Invalid random number generated");
    return r;
}

void
int_sort(int * arr, const int len)
{
    qsort(arr, len, sizeof(arr[0]), int_cmp_cb);
}

int *
rand_intarr_new(const int len)
{
    int *arr = malloc(sizeof(int) * len);

    if (arr)
    {
        int i;
        for (i = 0; i < len; ++i)
        {
            arr[i] = rand_int_range(0, RAND_MAX);
        }


        bool hassame = true;
        while (hassame)
        {
            hassame = false;
            int_sort(arr, len);
            for (i = 1; i < len; ++i)
            {
                if (arr[i - 1] == arr[i])
                {
                    arr[i] = rand_int_range(0, RAND_MAX);
                    hassame = true;
                }
            }
        }

        for (i = 0; i < len; ++i)
        {
            int swap = rand_int_range(0, len - 1);
            if (swap != i)
            {
                int tmp = arr[i];
                arr[i] = arr[swap];
                arr[swap] = tmp;
            }
        }
    }

    return arr;
}

void
rand_intarr_free(int *arr)
{
    if (arr)
    {
        free(arr);
    }
}

