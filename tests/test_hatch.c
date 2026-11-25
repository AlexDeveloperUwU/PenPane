/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/hatch.h"

#define MASK_SIZE 96

/**
 * @brief Applies a fill style to a fully covered mask and measures the result.
 * @param fillStyle Fill style to apply.
 * @param ink Receives the sum of all coverage values.
 * @param empty Receives the number of pixels left without coverage.
 * @param full Receives the number of pixels with full coverage.
 */
static void measure(FillStyle fillStyle, long *ink, int *empty, int *full)
{
    Mask mask;
    assert(mask_create(&mask, 10, 20, MASK_SIZE, MASK_SIZE));
    memset(mask.coverage, 255, (size_t)MASK_SIZE * MASK_SIZE);
    mask_apply_hatch(&mask, (Point){10.0f, 20.0f}, fillStyle, 2.0f);
    *ink = 0;
    *empty = 0;
    *full = 0;
    for (int i = 0; i < MASK_SIZE * MASK_SIZE; i++) {
        *ink += mask.coverage[i];
        *empty += mask.coverage[i] == 0;
        *full += mask.coverage[i] == 255;
    }
    mask_destroy(&mask);
}

/**
 * @brief Solid keeps everything, hachure leaves stripes, cross-hatch inks more than hachure.
 */
static void test_fill_styles(void)
{
    long solid;
    long hachure;
    long cross;
    int empty;
    int full;
    measure(FILL_STYLE_SOLID, &solid, &empty, &full);
    assert(empty == 0 && full == MASK_SIZE * MASK_SIZE);
    measure(FILL_STYLE_HACHURE, &hachure, &empty, &full);
    assert(empty > 0 && hachure > 0 && hachure < solid / 2);
    measure(FILL_STYLE_CROSS_HATCH, &cross, &empty, &full);
    assert(empty > 0 && cross > hachure && cross < solid / 2);
}

/**
 * @brief Uncovered pixels stay uncovered, and the pattern depends on the anchor, not the mask position.
 */
static void test_pattern_follows_anchor(void)
{
    Mask first;
    Mask second;
    assert(mask_create(&first, 0, 0, 40, 40));
    assert(mask_create(&second, 50, 70, 40, 40));
    memset(first.coverage, 255, 40 * 40);
    memset(second.coverage, 255, 40 * 40);
    second.coverage[0] = 0;
    mask_apply_hatch(&first, (Point){5.0f, 5.0f}, FILL_STYLE_HACHURE, 2.0f);
    mask_apply_hatch(&second, (Point){55.0f, 75.0f}, FILL_STYLE_HACHURE, 2.0f);
    assert(second.coverage[0] == 0);
    assert(memcmp(first.coverage + 1, second.coverage + 1, 39) == 0);
    mask_destroy(&first);
    mask_destroy(&second);
}

int main(void)
{
    test_fill_styles();
    test_pattern_follows_anchor();
    printf("hatch tests passed\n");
    return 0;
}
