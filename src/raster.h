/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file raster.h
 * @brief Anti-aliased shape rasterization into 8-bit coverage masks.
 *
 * Shapes are rendered into a Mask using signed distance fields, then the mask is blended onto a
 * canvas in one pass. Accumulating a whole stroke into one mask with a maximum operator avoids
 * the double darkening that overlapping segments would otherwise cause.
 */
#ifndef PENPANE_RASTER_H
#define PENPANE_RASTER_H

#include <stdbool.h>
#include <stdint.h>

#include "canvas.h"

/** @brief Floating point position. */
typedef struct Point {
    float x;
    float y;
} Point;

/** @brief Coverage buffer positioned in canvas coordinates. */
typedef struct Mask {
    int x;
    int y;
    int width;
    int height;
    uint8_t *coverage;
} Mask;

/**
 * @brief Allocates a zeroed mask covering the given canvas-space rectangle.
 * @param mask Mask to initialize.
 * @param x Left edge in canvas coordinates.
 * @param y Top edge in canvas coordinates.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @return true on success, false when memory is unavailable or the size is not positive.
 */
bool mask_create(Mask *mask, int x, int y, int width, int height);

/**
 * @brief Frees the mask buffer and zeroes the mask.
 * @param mask Mask to release.
 */
void mask_destroy(Mask *mask);

/**
 * @brief Adds a line segment with round caps and a linearly varying radius.
 * @param mask Target mask.
 * @param from Segment start.
 * @param to Segment end.
 * @param radiusFrom Radius at the start in pixels.
 * @param radiusTo Radius at the end in pixels.
 */
void mask_add_capsule(Mask *mask, Point from, Point to, float radiusFrom, float radiusTo);

/**
 * @brief Adds a filled polygon using the even-odd rule with exact horizontal and four-fold vertical coverage.
 * @param mask Target mask.
 * @param points Polygon vertices; the last vertex connects back to the first.
 * @param count Number of vertices, at least three.
 */
void mask_add_polygon_fill(Mask *mask, const Point *points, int count);

/**
 * @brief Adds a filled ellipse.
 * @param mask Target mask.
 * @param center Ellipse center.
 * @param radiusX Horizontal radius.
 * @param radiusY Vertical radius.
 */
void mask_add_ellipse_fill(Mask *mask, Point center, float radiusX, float radiusY);

/**
 * @brief Adds an ellipse outline.
 * @param mask Target mask.
 * @param center Ellipse center.
 * @param radiusX Horizontal radius of the stroke centerline.
 * @param radiusY Vertical radius of the stroke centerline.
 * @param strokeWidth Stroke width in pixels.
 */
void mask_add_ellipse_stroke(Mask *mask, Point center, float radiusX, float radiusY, float strokeWidth);

/**
 * @brief Adds a filled rounded rectangle.
 * @param mask Target mask.
 * @param left Left edge.
 * @param top Top edge.
 * @param width Width.
 * @param height Height.
 * @param cornerRadius Corner radius, clamped to half the shorter side.
 */
void mask_add_rect_fill(Mask *mask, float left, float top, float width, float height, float cornerRadius);

/**
 * @brief Adds a rounded rectangle outline.
 * @param mask Target mask.
 * @param left Left edge of the stroke centerline.
 * @param top Top edge of the stroke centerline.
 * @param width Width of the centerline rectangle.
 * @param height Height of the centerline rectangle.
 * @param cornerRadius Corner radius of the centerline.
 * @param strokeWidth Stroke width in pixels.
 */
void mask_add_rect_stroke(Mask *mask, float left, float top, float width, float height, float cornerRadius,
                          float strokeWidth);

/**
 * @brief Adds a soft drop shadow of a rounded rectangle.
 * @param mask Target mask.
 * @param left Left edge of the casting rectangle.
 * @param top Top edge of the casting rectangle.
 * @param width Width of the casting rectangle.
 * @param height Height of the casting rectangle.
 * @param cornerRadius Corner radius of the casting rectangle.
 * @param blur Blur distance in pixels.
 */
void mask_add_rect_shadow(Mask *mask, float left, float top, float width, float height, float cornerRadius,
                          float blur);

/**
 * @brief Blends a mask onto a canvas and leaves the mask unchanged.
 * @param canvas Target canvas.
 * @param mask Source coverage.
 * @param color Straight-alpha color.
 */
void mask_blend(Canvas *canvas, const Mask *mask, Color color);

#endif
