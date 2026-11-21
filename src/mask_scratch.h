/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file mask_scratch.h
 * @brief A single reusable coverage buffer so rendering does not allocate per element.
 */
#ifndef PENPANE_MASK_SCRATCH_H
#define PENPANE_MASK_SCRATCH_H

#include <stdbool.h>
#include <windows.h>

#include "raster.h"

/**
 * @brief Borrows the shared scratch mask, zeroed, covering a rectangle.
 *
 * The buffer is reused between calls and grows on demand. Only one borrower may be active at a
 * time; borrowing again invalidates the previous mask.
 * @param mask Receives a mask that aliases the shared buffer; never pass it to mask_destroy.
 * @param area Canvas rectangle to cover.
 * @return false when the rectangle is empty or memory ran out.
 */
bool mask_scratch_acquire(Mask *mask, RECT area);

/**
 * @brief Frees the shared buffer.
 */
void mask_scratch_release(void);

/**
 * @brief Frees the shared buffer when it has grown beyond a size worth keeping.
 * @param keepBytes Largest capacity that is kept for the next borrower.
 */
void mask_scratch_trim(size_t keepBytes);

#endif
