#include "raster.h"

#include <math.h>
#include <stdlib.h>

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
static float clamp_unit(float value)
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
static float capsule_distance(const void *context, float x, float y)
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
static float ellipse_distance(const void *context, float x, float y)
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
static float rect_distance(const void *context, float x, float y)
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
 * @brief Writes the maximum of the existing and new coverage over a pixel window.
 * @param mask Target mask.
 * @param left Window left in canvas coordinates.
 * @param top Window top in canvas coordinates.
 * @param right Window right (exclusive) in canvas coordinates.
 * @param bottom Window bottom (exclusive) in canvas coordinates.
 * @param distance Distance function.
 * @param context Shape parameters for the distance function.
 * @param strokeHalfWidth Half stroke width, or a negative value to fill the interior.
 * @param shadowBlur Blur distance for a shadow falloff, or zero for none.
 */
static void rasterize(Mask *mask, int left, int top, int right, int bottom,
                      DistanceFunction distance, const void *context, float strokeHalfWidth,
                      float shadowBlur)
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
    for (int y = top; y < bottom; y++) {
        uint8_t *row = mask->coverage + (size_t)(y - mask->y) * mask->width - mask->x;
        for (int x = left; x < right; x++) {
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
    float minX = (from.x < to.x ? from.x : to.x) - reach;
    float maxX = (from.x > to.x ? from.x : to.x) + reach;
    float minY = (from.y < to.y ? from.y : to.y) - reach;
    float maxY = (from.y > to.y ? from.y : to.y) + reach;
    rasterize(mask, (int)floorf(minX), (int)floorf(minY), (int)ceilf(maxX), (int)ceilf(maxY),
              capsule_distance, &shape, -1.0f, 0.0f);
}

void mask_add_ellipse_fill(Mask *mask, Point center, float radiusX, float radiusY)
{
    EllipseShape shape = {center, radiusX, radiusY};
    rasterize(mask, (int)floorf(center.x - radiusX - 1.0f), (int)floorf(center.y - radiusY - 1.0f),
              (int)ceilf(center.x + radiusX + 1.0f), (int)ceilf(center.y + radiusY + 1.0f),
              ellipse_distance, &shape, -1.0f, 0.0f);
}

void mask_add_ellipse_stroke(Mask *mask, Point center, float radiusX, float radiusY,
                             float strokeWidth)
{
    EllipseShape shape = {center, radiusX, radiusY};
    float reach = strokeWidth * 0.5f + 1.0f;
    rasterize(mask, (int)floorf(center.x - radiusX - reach), (int)floorf(center.y - radiusY - reach),
              (int)ceilf(center.x + radiusX + reach), (int)ceilf(center.y + radiusY + reach),
              ellipse_distance, &shape, strokeWidth * 0.5f, 0.0f);
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
    rasterize(mask, (int)floorf(left - 1.0f), (int)floorf(top - 1.0f), (int)ceilf(left + width + 1.0f),
              (int)ceilf(top + height + 1.0f), rect_distance, &shape, -1.0f, 0.0f);
}

void mask_add_rect_stroke(Mask *mask, float left, float top, float width, float height,
                          float cornerRadius, float strokeWidth)
{
    RectShape shape = make_rect(left, top, width, height, cornerRadius);
    float reach = strokeWidth * 0.5f + 1.0f;
    rasterize(mask, (int)floorf(left - reach), (int)floorf(top - reach),
              (int)ceilf(left + width + reach), (int)ceilf(top + height + reach), rect_distance,
              &shape, strokeWidth * 0.5f, 0.0f);
}

void mask_add_rect_shadow(Mask *mask, float left, float top, float width, float height,
                          float cornerRadius, float blur)
{
    RectShape shape = make_rect(left, top, width, height, cornerRadius);
    rasterize(mask, (int)floorf(left - blur), (int)floorf(top - blur),
              (int)ceilf(left + width + blur), (int)ceilf(top + height + blur), rect_distance,
              &shape, -1.0f, blur);
}

void mask_blend(Canvas *canvas, const Mask *mask, Color color)
{
    canvas_blend_coverage(canvas, mask->x, mask->y, mask->coverage, mask->width, mask->height,
                          color);
}
