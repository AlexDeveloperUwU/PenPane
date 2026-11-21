/**
 * @file shapes_ui.h
 * @brief Small drawing helpers for the UI built on the rasterizer: a reusable scratch mask and
 *        one-call fills, strokes, arcs and polylines.
 */
#ifndef INK_SHAPES_UI_H
#define INK_SHAPES_UI_H

#include <stdbool.h>

#include "raster.h"

/**
 * @brief Returns a zeroed scratch mask for the given canvas rectangle, reusing one buffer.
 *
 * The mask stays valid until the next call; callers blend it immediately and never keep it.
 * @param x Left edge in canvas coordinates.
 * @param y Top edge in canvas coordinates.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @return Scratch mask, or NULL when the size is not positive or memory is unavailable.
 */
Mask *ui_scratch_mask(int x, int y, int width, int height);

/**
 * @brief Frees the scratch buffer.
 */
void ui_scratch_release(void);

/**
 * @brief Fills a rounded rectangle with a straight-alpha color.
 * @param canvas Target canvas.
 * @param left Left edge.
 * @param top Top edge.
 * @param width Width.
 * @param height Height.
 * @param radius Corner radius.
 * @param color Straight-alpha color.
 */
void ui_fill_round_rect(Canvas *canvas, float left, float top, float width, float height,
                        float radius, Color color);

/**
 * @brief Strokes a rounded rectangle outline whose centerline is the given rectangle.
 * @param canvas Target canvas.
 * @param left Left edge of the centerline.
 * @param top Top edge of the centerline.
 * @param width Width of the centerline rectangle.
 * @param height Height of the centerline rectangle.
 * @param radius Corner radius of the centerline.
 * @param stroke Stroke width.
 * @param color Straight-alpha color.
 */
void ui_stroke_round_rect(Canvas *canvas, float left, float top, float width, float height,
                          float radius, float stroke, Color color);

/**
 * @brief Fills a circle.
 * @param canvas Target canvas.
 * @param centerX Center x.
 * @param centerY Center y.
 * @param radius Radius.
 * @param color Straight-alpha color.
 */
void ui_fill_circle(Canvas *canvas, float centerX, float centerY, float radius, Color color);

/**
 * @brief Adds a connected polyline with round joins to a mask.
 * @param mask Target mask.
 * @param points Vertices.
 * @param count Number of vertices.
 * @param closed true to connect the last vertex to the first.
 * @param radius Half stroke width.
 */
void ui_mask_polyline(Mask *mask, const Point *points, int count, bool closed, float radius);

/**
 * @brief Adds a circular arc stroke to a mask.
 * @param mask Target mask.
 * @param center Arc center.
 * @param arcRadius Radius of the arc centerline.
 * @param startDegrees Start angle in degrees, 0 pointing right and 90 pointing down.
 * @param endDegrees End angle in degrees.
 * @param radius Half stroke width.
 */
void ui_mask_arc(Mask *mask, Point center, float arcRadius, float startDegrees, float endDegrees,
                 float radius);

#endif
