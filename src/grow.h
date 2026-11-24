/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file grow.h
 * @brief Capacity doubling for the dynamic arrays used by the model.
 */
#ifndef PENPANE_GROW_H
#define PENPANE_GROW_H

#include <stddef.h>

/**
 * @brief Makes room for one more element, doubling the capacity when the array is full.
 * @param array Current heap array, or NULL when empty.
 * @param count Number of elements in use.
 * @param capacity Address of the current capacity; updated when the array grows.
 * @param elementSize Size of one element in bytes.
 * @param initialCapacity Capacity to start from when the array is empty.
 * @return The array to use from now on, or NULL when the allocation failed (the old array stays valid).
 */
void *array_reserve_one(void *array, int count, int *capacity, size_t elementSize, int initialCapacity);

#endif
