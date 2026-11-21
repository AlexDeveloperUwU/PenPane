/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define POLYGON_SUBSAMPLES 4
#define SPAN_MARGIN 0.02f

/**
 * @brief Signed distance function evaluated per pixel center.
 * @param context Shape parameters.
 * @param x Pixel center x in canvas coordinates.
 * @param y Pixel center y in canvas coordinates.
 * @return Signed distance, negative inside the shape.
 */
typedef float (*DistanceFunction)(const void *context, float x, float y);

/** @brief Parameters of a capsule with varying radius. */
typedef struct CapsuleShape {
    Point from;
    Point to;
    float radiusFrom;
    float radiusTo;
} CapsuleShape;

/** @brief Parameters of an ellipse centered at a point. */
typedef struct EllipseShape {
    Point center;
    float radiusX;
    float radiusY;
} EllipseShape;

/** @brief Parameters of a rounded rectangle given by center and half extents. */
typedef struct RectShape {
    Point center;
    float halfWidth;
    float halfHeight;
    float cornerRadius;
} RectShape;

/**
 * @brief Clamps a float to the unit interval.
 * @param value Input value.
 * @return Value limited to the range 0 to 1.
 */
static inline __attribute__((always_inline)) float clamp_unit(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

/**
 * @brief Distance from a pixel to a capsule, minus the interpolated radius.
 * @param context CapsuleShape pointer.
 * @param x Pixel x.
 * @param y Pixel y.
 * @return Signed distance to the capsule boundary.
 */
static inline __attribute__((always_inline)) float capsule_distance(const void *context, float x, float y)
{
    const CapsuleShape *shape = context;
    float dx = shape->to.x - shape->from.x;
    float dy = shape->to.y - shape->from.y;
    float lengthSquared = dx * dx + dy * dy;
    float t = lengthSquared > 0.0f
                  ? clamp_unit(((x - shape->from.x) * dx + (y - shape->from.y) * dy) / lengthSquared)
                  : 0.0f;
    float px = shape->from.x + dx * t - x;
    float py = shape->from.y + dy * t - y;
    float radius = shape->radiusFrom + (shape->radiusTo - shape->radiusFrom) * t;
    return sqrtf(px * px + py * py) - radius;
}

/**
 * @brief Approximate signed distance to an ellipse.
 * @param context EllipseShape pointer.
 * @param x Pixel x.
 * @param y Pixel y.
 * @return Signed distance, accurate near the boundary.
 */
static inline __attribute__((always_inline)) float ellipse_distance(const void *context, float x, float y)
{
    const EllipseShape *shape = context;
    float nx = (x - shape->center.x) / shape->radiusX;
    float ny = (y - shape->center.y) / shape->radiusY;
    float k0 = sqrtf(nx * nx + ny * ny);
    float gx = nx / shape->radiusX;
    float gy = ny / shape->radiusY;
    float k1 = sqrtf(gx * gx + gy * gy);
    if (k1 < 1e-9f) {
        return -(shape->radiusX < shape->radiusY ? shape->radiusX : shape->radiusY);
    }
    return k0 * (k0 - 1.0f) / k1;
}

/**
 * @brief Signed distance to a rounded rectangle.
 * @param context RectShape pointer.
 * @param x Pixel x.
 * @param y Pixel y.
 * @return Signed distance to the boundary.
 */
static inline __attribute__((always_inline)) float rect_distance(const void *context, float x, float y)
{
    const RectShape *shape = context;
    float qx = fabsf(x - shape->center.x) - (shape->halfWidth - shape->cornerRadius);
    float qy = fabsf(y - shape->center.y) - (shape->halfHeight - shape->cornerRadius);
    float outsideX = qx > 0.0f ? qx : 0.0f;
    float outsideY = qy > 0.0f ? qy : 0.0f;
    float inside = qx > qy ? qx : qy;
    return sqrtf(outsideX * outsideX + outsideY * outsideY) + (inside < 0.0f ? inside : 0.0f) -
           shape->cornerRadius;
}

/**
 * @brief Finds the pixels of one row that lie so deep inside a shape that their coverage is known.
 * @param context Shape parameters.
 * @param yCenter Row center y in canvas coordinates.
 * @param threshold Inside distance beyond which a pixel is certain to be fully inside or outside the band.
 * @param first Receives the first certain pixel.
 * @param last Receives one past the last certain pixel; not greater than first when there is none.
 */
typedef void (*SpanFunction)(const void *context, float yCenter, float threshold, int *first,
                             int *last);

/**
 * @brief Interior span of an ellipse row, from the guaranteed bound distance <= -(1 - k0) * min radius.
 * @param context EllipseShape pointer.
 * @param yCenter Row center y.
 * @param threshold Required inside distance.
 * @param first Receives the first certain pixel.
 * @param last Receives one past the last certain pixel.
 */
static inline __attribute__((always_inline)) void ellipse_span(const void *context, float yCenter,
                                                              float threshold, int *first,
                                                              int *last)
{
    const EllipseShape *shape = context;
    *first = *last = 0;
    float smallest = shape->radiusX < shape->radiusY ? shape->radiusX : shape->radiusY;
    float limit = 1.0f - threshold / smallest;
    if (limit <= 0.0f) {
        return;
    }
    float ny = (yCenter - shape->center.y) / shape->radiusY;
    float rest = limit * limit - ny * ny;
    if (rest <= 0.0f) {
        return;
    }
    float reach = sqrtf(rest) * shape->radiusX;
    *first = (int)ceilf(shape->center.x - reach - 0.5f);
    *last = (int)floorf(shape->center.x + reach - 0.5f) + 1;
}

/**
 * @brief Interior span of a rounded rectangle row.
 * @param context RectShape pointer.
 * @param yCenter Row center y.
 * @param threshold Required inside distance.
 * @param first Receives the first certain pixel.
 * @param last Receives one past the last certain pixel.
 */
static inline __attribute__((always_inline)) void rect_span(const void *context, float yCenter,
                                                           float threshold, int *first, int *last)
{
    const RectShape *shape = context;
    *first = *last = 0;
    float qy = fabsf(yCenter - shape->center.y) - (shape->halfHeight - shape->cornerRadius);
    if (qy >= shape->cornerRadius - threshold) {
        return;
    }
    float reach = qy <= 0.0f ? shape->halfWidth - threshold : shape->halfWidth - shape->cornerRadius;
    if (reach <= 0.0f) {
        return;
    }
    *first = (int)ceilf(shape->center.x - reach - 0.5f);
    *last = (int)floorf(shape->center.x + reach - 0.5f) + 1;
}

/**
 * @brief Evaluates the distance field for a run of pixels of one row and keeps the maximum coverage.
 * @param row Mask row pointer indexed by canvas x.
 * @param y Row in canvas coordinates.
 * @param from First pixel.
 * @param to One past the last pixel.
 * @param distance Distance function.
 * @param context Shape parameters.
 * @param strokeHalfWidth Half stroke width, or a negative value to fill the interior.
 * @param shadowBlur Blur distance for a shadow falloff, or zero for none.
 */
static inline __attribute__((always_inline)) void rasterize_run(uint8_t *row, int y, int from, int to,
                                                               DistanceFunction distance,
                                                               const void *context,
                                                               float strokeHalfWidth,
                                                               float shadowBlur)
{
    for (int x = from; x < to; x++) {
        float d = distance(context, (float)x + 0.5f, (float)y + 0.5f);
        float cover;
        if (shadowBlur > 0.0f) {
            cover = clamp_unit(0.5f - d / shadowBlur);
            cover *= cover;
        } else if (strokeHalfWidth >= 0.0f) {
            cover = clamp_unit(strokeHalfWidth + 0.5f - fabsf(d));
        } else {
            cover = clamp_unit(0.5f - d);
        }
        uint8_t value = (uint8_t)(cover * 255.0f + 0.5f);
        if (value > row[x]) {
            row[x] = value;
        }
    }
}

/**
 * @brief Writes the maximum of the existing and new coverage over a pixel window.
 * @param mask Target mask.
 * @param left Window left in canvas coordinates.
 * @param top Window top in canvas coordinates.
 * @param right Window right (exclusive) in canvas coordinates.
 * @param bottom Window bottom (exclusive) in canvas coordinates.
 * @param distance Distance function.
 * @param span Interior span function that lets deep interior pixels skip the distance field, or NULL.
 * @param context Shape parameters for the distance function.
 * @param strokeHalfWidth Half stroke width, or a negative value to fill the interior.
 * @param shadowBlur Blur distance for a shadow falloff, or zero for none.
 */
static inline __attribute__((always_inline)) void rasterize_inline(
    Mask *mask, int left, int top, int right, int bottom, DistanceFunction distance,
    SpanFunction span, const void *context, float strokeHalfWidth, float shadowBlur)
{
    if (left < mask->x) {
        left = mask->x;
    }
    if (top < mask->y) {
        top = mask->y;
    }
    if (right > mask->x + mask->width) {
        right = mask->x + mask->width;
    }
    if (bottom > mask->y + mask->height) {
        bottom = mask->y + mask->height;
    }
    float threshold = (strokeHalfWidth >= 0.0f ? strokeHalfWidth + 0.5f : 0.5f) + SPAN_MARGIN;
    for (int y = top; y < bottom; y++) {
        uint8_t *row = mask->coverage + (size_t)(y - mask->y) * mask->width - mask->x;
        int skipFrom = right;
        int skipTo = right;
        if (span && shadowBlur <= 0.0f) {
            span(context, (float)y + 0.5f, threshold, &skipFrom, &skipTo);
            if (skipTo <= skipFrom) {
                skipFrom = skipTo = right;
            }
            skipFrom = skipFrom < left ? left : (skipFrom > right ? right : skipFrom);
            skipTo = skipTo < skipFrom ? skipFrom : (skipTo > right ? right : skipTo);
        }
        rasterize_run(row, y, left, skipFrom, distance, context, strokeHalfWidth, shadowBlur);
        if (strokeHalfWidth < 0.0f) {
            for (int x = skipFrom; x < skipTo; x++) {
                row[x] = 255;
            }
        }
        rasterize_run(row, y, skipTo, right, distance, context, strokeHalfWidth, shadowBlur);
    }
}

/**
 * @brief Rasterizes a capsule distance field over a pixel window.
 * @param mask Target mask.
 * @param left Window left in canvas coordinates.
 * @param top Window top in canvas coordinates.
 * @param right Window right (exclusive) in canvas coordinates.
 * @param bottom Window bottom (exclusive) in canvas coordinates.
 * @param context CapsuleShape pointer.
 */
static void rasterize_capsule(Mask *mask, int left, int top, int right, int bottom,
                              const void *context)
{
    rasterize_inline(mask, left, top, right, bottom, capsule_distance, NULL, context, -1.0f, 0.0f);
}

/**
 * @brief Rasterizes an ellipse distance field over a pixel window.
 * @param mask Target mask.
 * @param left Window left in canvas coordinates.
 * @param top Window top in canvas coordinates.
 * @param right Window right (exclusive) in canvas coordinates.
 * @param bottom Window bottom (exclusive) in canvas coordinates.
 * @param context EllipseShape pointer.
 * @param strokeHalfWidth Half stroke width, or a negative value to fill the interior.
 */
static void rasterize_ellipse(Mask *mask, int left, int top, int right, int bottom,
                              const void *context, float strokeHalfWidth)
{
    rasterize_inline(mask, left, top, right, bottom, ellipse_distance, ellipse_span, context,
                     strokeHalfWidth,
                     0.0f);
}

/**
 * @brief Rasterizes a rounded rectangle distance field over a pixel window.
 * @param mask Target mask.
 * @param left Window left in canvas coordinates.
 * @param top Window top in canvas coordinates.
 * @param right Window right (exclusive) in canvas coordinates.
 * @param bottom Window bottom (exclusive) in canvas coordinates.
 * @param context RectShape pointer.
 * @param strokeHalfWidth Half stroke width, or a negative value to fill the interior.
 * @param shadowBlur Blur distance for a shadow falloff, or zero for none.
 */
static void rasterize_rect(Mask *mask, int left, int top, int right, int bottom,
                           const void *context, float strokeHalfWidth, float shadowBlur)
{
    rasterize_inline(mask, left, top, right, bottom, rect_distance, rect_span, context,
                     strokeHalfWidth,
                     shadowBlur);
}

bool mask_create(Mask *mask, int x, int y, int width, int height)
{
    mask->coverage = NULL;
    mask->width = mask->height = 0;
    if (width <= 0 || height <= 0) {
        return false;
    }
    mask->coverage = calloc((size_t)width * height, 1);
    if (!mask->coverage) {
        return false;
    }
    mask->x = x;
    mask->y = y;
    mask->width = width;
    mask->height = height;
    return true;
}

void mask_destroy(Mask *mask)
{
    free(mask->coverage);
    mask->coverage = NULL;
    mask->width = mask->height = 0;
}

void mask_add_capsule(Mask *mask, Point from, Point to, float radiusFrom, float radiusTo)
{
    CapsuleShape shape = {from, to, radiusFrom, radiusTo};
    float reach = (radiusFrom > radiusTo ? radiusFrom : radiusTo) + 1.0f;
    float minY = (from.y < to.y ? from.y : to.y) - reach;
    float maxY = (from.y > to.y ? from.y : to.y) + reach;
    int top = (int)floorf(minY);
    int bottom = (int)ceilf(maxY);
    if (top < mask->y) {
        top = mask->y;
    }
    if (bottom > mask->y + mask->height) {
        bottom = mask->y + mask->height;
    }
    float dy = to.y - from.y;
    for (int y = top; y < bottom; y++) {
        float centerY = (float)y + 0.5f;
        float t0 = 0.0f;
        float t1 = 1.0f;
        if (fabsf(dy) > 1e-6f) {
            float a = (centerY - reach - from.y) / dy;
            float b = (centerY + reach - from.y) / dy;
            t0 = clamp_unit(a < b ? a : b);
            t1 = clamp_unit(a < b ? b : a);
            if ((a < 0.0f && b < 0.0f) || (a > 1.0f && b > 1.0f)) {
                continue;
            }
        } else if (fabsf(centerY - from.y) > reach) {
            continue;
        }
        float xa = from.x + (to.x - from.x) * t0;
        float xb = from.x + (to.x - from.x) * t1;
        int left = (int)floorf((xa < xb ? xa : xb) - reach);
        int right = (int)ceilf((xa < xb ? xb : xa) + reach);
        rasterize_capsule(mask, left, y, right, y + 1, &shape);
    }
}

void mask_add_polygon_fill(Mask *mask, const Point *points, int count)
{
    if (count < 3 || mask->width <= 0) {
        return;
    }
    float minY = points[0].y;
    float maxY = points[0].y;
    for (int i = 1; i < count; i++) {
        minY = points[i].y < minY ? points[i].y : minY;
        maxY = points[i].y > maxY ? points[i].y : maxY;
    }
    int top = (int)floorf(minY);
    int bottom = (int)ceilf(maxY);
    top = top < mask->y ? mask->y : top;
    bottom = bottom > mask->y + mask->height ? mask->y + mask->height : bottom;
    float *accumulator = calloc((size_t)mask->width, sizeof(float));
    float *crossings = malloc((size_t)count * sizeof(float));
    if (!accumulator || !crossings) {
        free(accumulator);
        free(crossings);
        return;
    }
    for (int y = top; y < bottom; y++) {
        memset(accumulator, 0, (size_t)mask->width * sizeof(float));
        for (int sub = 0; sub < POLYGON_SUBSAMPLES; sub++) {
            float sampleY = (float)y + ((float)sub + 0.5f) / (float)POLYGON_SUBSAMPLES;
            int found = 0;
            for (int i = 0; i < count; i++) {
                Point a = points[i];
                Point b = points[(i + 1) % count];
                if ((a.y <= sampleY) != (b.y <= sampleY)) {
                    crossings[found++] = a.x + (sampleY - a.y) / (b.y - a.y) * (b.x - a.x);
                }
            }
            for (int i = 1; i < found; i++) {
                float value = crossings[i];
                int j = i - 1;
                while (j >= 0 && crossings[j] > value) {
                    crossings[j + 1] = crossings[j];
                    j--;
                }
                crossings[j + 1] = value;
            }
            for (int i = 0; i + 1 < found; i += 2) {
                float x0 = crossings[i] - (float)mask->x;
                float x1 = crossings[i + 1] - (float)mask->x;
                x0 = x0 < 0.0f ? 0.0f : x0;
                x1 = x1 > (float)mask->width ? (float)mask->width : x1;
                if (x1 <= x0) {
                    continue;
                }
                int first = (int)floorf(x0);
                int last = (int)floorf(x1);
                if (first == last) {
                    accumulator[first] += x1 - x0;
                    continue;
                }
                accumulator[first] += (float)(first + 1) - x0;
                for (int x = first + 1; x < last; x++) {
                    accumulator[x] += 1.0f;
                }
                if (last < mask->width) {
                    accumulator[last] += x1 - (float)last;
                }
            }
        }
        uint8_t *row = mask->coverage + (size_t)(y - mask->y) * mask->width;
        for (int x = 0; x < mask->width; x++) {
            float cover = accumulator[x] / (float)POLYGON_SUBSAMPLES;
            uint8_t value = (uint8_t)(clamp_unit(cover) * 255.0f + 0.5f);
            if (value > row[x]) {
                row[x] = value;
            }
        }
    }
    free(accumulator);
    free(crossings);
}

void mask_add_ellipse_fill(Mask *mask, Point center, float radiusX, float radiusY)
{
    EllipseShape shape = {center, radiusX, radiusY};
    rasterize_ellipse(mask, (int)floorf(center.x - radiusX - 1.0f), (int)floorf(center.y - radiusY - 1.0f),
              (int)ceilf(center.x + radiusX + 1.0f), (int)ceilf(center.y + radiusY + 1.0f),
              &shape, -1.0f);
}

void mask_add_ellipse_stroke(Mask *mask, Point center, float radiusX, float radiusY,
                             float strokeWidth)
{
    EllipseShape shape = {center, radiusX, radiusY};
    float reach = strokeWidth * 0.5f + 1.0f;
    rasterize_ellipse(mask, (int)floorf(center.x - radiusX - reach), (int)floorf(center.y - radiusY - reach),
              (int)ceilf(center.x + radiusX + reach), (int)ceilf(center.y + radiusY + reach),
              &shape, strokeWidth * 0.5f);
}

/**
 * @brief Builds a RectShape from edges and clamps the corner radius.
 * @param left Left edge.
 * @param top Top edge.
 * @param width Width.
 * @param height Height.
 * @param cornerRadius Requested corner radius.
 * @return Rectangle shape with center and half extents.
 */
static RectShape make_rect(float left, float top, float width, float height, float cornerRadius)
{
    RectShape shape;
    shape.center.x = left + width * 0.5f;
    shape.center.y = top + height * 0.5f;
    shape.halfWidth = width * 0.5f;
    shape.halfHeight = height * 0.5f;
    float limit = shape.halfWidth < shape.halfHeight ? shape.halfWidth : shape.halfHeight;
    shape.cornerRadius = cornerRadius > limit ? limit : (cornerRadius < 0.0f ? 0.0f : cornerRadius);
    return shape;
}

void mask_add_rect_fill(Mask *mask, float left, float top, float width, float height,
                        float cornerRadius)
{
    RectShape shape = make_rect(left, top, width, height, cornerRadius);
    rasterize_rect(mask, (int)floorf(left - 1.0f), (int)floorf(top - 1.0f), (int)ceilf(left + width + 1.0f),
              (int)ceilf(top + height + 1.0f), &shape, -1.0f, 0.0f);
}

void mask_add_rect_stroke(Mask *mask, float left, float top, float width, float height,
                          float cornerRadius, float strokeWidth)
{
    RectShape shape = make_rect(left, top, width, height, cornerRadius);
    float reach = strokeWidth * 0.5f + 1.0f;
    rasterize_rect(mask, (int)floorf(left - reach), (int)floorf(top - reach),
              (int)ceilf(left + width + reach), (int)ceilf(top + height + reach), &shape, strokeWidth * 0.5f, 0.0f);
}

void mask_add_rect_shadow(Mask *mask, float left, float top, float width, float height,
                          float cornerRadius, float blur)
{
    RectShape shape = make_rect(left, top, width, height, cornerRadius);
    rasterize_rect(mask, (int)floorf(left - blur), (int)floorf(top - blur),
              (int)ceilf(left + width + blur), (int)ceilf(top + height + blur), &shape, -1.0f, blur);
}

void mask_blend(Canvas *canvas, const Mask *mask, Color color)
{
    canvas_blend_coverage(canvas, mask->x, mask->y, mask->coverage, mask->width, mask->height,
                          color);
}
