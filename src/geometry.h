/**
 * @file geometry.h
 * @brief Small geometry helpers: dirty-region accumulation, rectangle math, segment distance.
 */
#ifndef INK_GEOMETRY_H
#define INK_GEOMETRY_H

#include <stdbool.h>
#include <windows.h>

#include "raster.h"

/** @brief Axis-aligned rectangle in floating point. */
typedef struct FloatRect {
    float left;
    float top;
    float right;
    float bottom;
} FloatRect;

/** @brief Union of changed rectangles awaiting repaint. */
typedef struct Dirty {
    RECT rect;
    bool empty;
} Dirty;

/**
 * @brief Marks a dirty region as empty.
 * @param dirty Region to reset.
 */
void dirty_reset(Dirty *dirty);

/**
 * @brief Grows a dirty region to include a rectangle.
 * @param dirty Region to grow.
 * @param rect Rectangle to include; ignored when empty.
 */
void dirty_add(Dirty *dirty, RECT rect);

/**
 * @brief Intersects two rectangles.
 * @param a First rectangle.
 * @param b Second rectangle.
 * @param result Receives the intersection when the function returns true.
 * @return true when the rectangles overlap with positive area.
 */
bool rect_intersect(RECT a, RECT b, RECT *result);

/**
 * @brief Grows a rectangle on every side.
 * @param rect Source rectangle.
 * @param amount Pixels to add on each side.
 * @return Inflated rectangle.
 */
RECT rect_inflate(RECT rect, int amount);

/**
 * @brief Converts a floating rectangle to the smallest enclosing integer rectangle.
 * @param box Source rectangle.
 * @param margin Extra pixels on every side.
 * @return Integer rectangle.
 */
RECT rect_from_float(FloatRect box, float margin);

/**
 * @brief Normalizes two corner points into an ordered rectangle.
 * @param a First corner.
 * @param b Opposite corner.
 * @return Rectangle with left <= right and top <= bottom.
 */
FloatRect float_rect_from_points(Point a, Point b);

/**
 * @brief Distance from a point to a segment.
 * @param point Query point.
 * @param from Segment start.
 * @param to Segment end.
 * @return Euclidean distance in pixels.
 */
float segment_distance(Point point, Point from, Point to);

#endif
