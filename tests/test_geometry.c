/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../src/geometry.h"
#include "../src/grow.h"

/**
 * @brief Rectangles that only touch or are apart do not intersect; overlapping ones return the overlap.
 */
static void test_intersect(void)
{
    RECT result;
    assert(rect_intersect((RECT){0, 0, 10, 10}, (RECT){5, 5, 20, 20}, &result));
    assert(result.left == 5 && result.top == 5 && result.right == 10 && result.bottom == 10);
    assert(!rect_intersect((RECT){0, 0, 10, 10}, (RECT){10, 0, 20, 10}, &result));
    assert(!rect_intersect((RECT){0, 0, 10, 10}, (RECT){30, 30, 40, 40}, &result));
}

/**
 * @brief Dirty regions start empty and grow to the union of what is added.
 */
static void test_dirty(void)
{
    Dirty dirty;
    dirty_reset(&dirty);
    assert(dirty.empty);
    dirty_add(&dirty, (RECT){10, 10, 20, 20});
    assert(!dirty.empty);
    dirty_add(&dirty, (RECT){50, 5, 60, 15});
    assert(dirty.rect.left == 10 && dirty.rect.top == 5 && dirty.rect.right == 60 && dirty.rect.bottom == 20);
    dirty_add(&dirty, (RECT){0, 0, 0, 0});
    assert(dirty.rect.left == 10);
}

/**
 * @brief Inflate grows every side; the float conversion rounds outward and adds the margin.
 */
static void test_rect_conversions(void)
{
    RECT inflated = rect_inflate((RECT){10, 10, 20, 20}, 3);
    assert(inflated.left == 7 && inflated.top == 7 && inflated.right == 23 && inflated.bottom == 23);
    RECT enclosing = rect_from_float((FloatRect){1.2f, 2.7f, 9.1f, 9.9f}, 1.0f);
    assert(enclosing.left == 0 && enclosing.top == 1 && enclosing.right == 11 && enclosing.bottom == 11);
    FloatRect ordered = float_rect_from_points((Point){50, 40}, (Point){10, 90});
    assert(ordered.left == 10 && ordered.top == 40 && ordered.right == 50 && ordered.bottom == 90);
}

/**
 * @brief Segment distance covers the interior, both ends and a degenerate segment.
 */
static void test_segment_distance(void)
{
    Point a = {0, 0};
    Point b = {10, 0};
    assert(fabsf(segment_distance((Point){5, 3}, a, b) - 3.0f) < 1e-5f);
    assert(fabsf(segment_distance((Point){-3, 4}, a, b) - 5.0f) < 1e-5f);
    assert(fabsf(segment_distance((Point){13, 4}, a, b) - 5.0f) < 1e-5f);
    assert(fabsf(segment_distance((Point){3, 4}, a, a) - 5.0f) < 1e-5f);
}

/**
 * @brief The grow helper starts at the initial capacity, doubles when full and keeps the contents.
 */
static void test_grow(void)
{
    int *values = NULL;
    int capacity = 0;
    for (int i = 0; i < 100; i++) {
        int *grown = array_reserve_one(values, i, &capacity, sizeof(int), 4);
        assert(grown);
        values = grown;
        values[i] = i;
        assert(capacity >= i + 1);
    }
    assert(capacity == 128);
    for (int i = 0; i < 100; i++) {
        assert(values[i] == i);
    }
    free(values);
}

int main(void)
{
    test_intersect();
    test_dirty();
    test_rect_conversions();
    test_segment_distance();
    test_grow();
    printf("geometry tests passed\n");
    return 0;
}
