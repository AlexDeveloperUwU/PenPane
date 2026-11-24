/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "grow.h"

#include <stdlib.h>

void *array_reserve_one(void *array, int count, int *capacity, size_t elementSize, int initialCapacity)
{
    if (count < *capacity) {
        return array;
    }
    int grown = *capacity ? *capacity * 2 : initialCapacity;
    void *replacement = realloc(array, elementSize * (size_t)grown);
    if (replacement) {
        *capacity = grown;
    }
    return replacement;
}
