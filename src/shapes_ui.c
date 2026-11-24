/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shapes_ui.h"

#include "geometry.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ARC_SEGMENT_DEGREES 12.0f

static Mask g_scratch;
static size_t g_scratchCapacity;

Mask *ui_scratch_mask(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    size_t needed = (size_t)width * (size_t)height;
    if (needed > g_scratchCapacity) {
        uint8_t *grown = realloc(g_scratch.coverage, needed);
        if (!grown) {
            return NULL;
        }
        g_scratch.coverage = grown;
        g_scratchCapacity = needed;
    }
    memset(g_scratch.coverage, 0, needed);
    g_scratch.x = x;
    g_scratch.y = y;
    g_scratch.width = width;
    g_scratch.height = height;
    return &g_scratch;
}

void ui_scratch_release(void)
{
    free(g_scratch.coverage);
    memset(&g_scratch, 0, sizeof g_scratch);
    g_scratchCapacity = 0;
}

void ui_fill_round_rect(Canvas *canvas, float left, float top, float width, float height, float radius,
                        Color color)
{
    int x = (int)floorf(left) - 1;
    int y = (int)floorf(top) - 1;
    Mask *mask = ui_scratch_mask(x, y, (int)ceilf(width) + 4, (int)ceilf(height) + 4);
    if (!mask) {
        return;
    }
    mask_add_rect_fill(mask, left, top, width, height, radius);
    mask_blend(canvas, mask, color);
}

void ui_stroke_round_rect(Canvas *canvas, float left, float top, float width, float height, float radius,
                          float stroke, Color color)
{
    int x = (int)floorf(left - stroke) - 1;
    int y = (int)floorf(top - stroke) - 1;
    Mask *mask =
        ui_scratch_mask(x, y, (int)ceilf(width + stroke * 2.0f) + 4, (int)ceilf(height + stroke * 2.0f) + 4);
    if (!mask) {
        return;
    }
    mask_add_rect_stroke(mask, left, top, width, height, radius, stroke);
    mask_blend(canvas, mask, color);
}

void ui_fill_circle(Canvas *canvas, float centerX, float centerY, float radius, Color color)
{
    int x = (int)floorf(centerX - radius) - 1;
    int y = (int)floorf(centerY - radius) - 1;
    Mask *mask = ui_scratch_mask(x, y, (int)ceilf(radius * 2.0f) + 4, (int)ceilf(radius * 2.0f) + 4);
    if (!mask) {
        return;
    }
    mask_add_ellipse_fill(mask, (Point){centerX, centerY}, radius, radius);
    mask_blend(canvas, mask, color);
}

void ui_mask_polyline(Mask *mask, const Point *points, int count, bool closed, float radius)
{
    for (int i = 0; i + 1 < count; i++) {
        mask_add_capsule(mask, points[i], points[i + 1], radius, radius);
    }
    if (closed && count > 2) {
        mask_add_capsule(mask, points[count - 1], points[0], radius, radius);
    }
}

void ui_mask_arc(Mask *mask, Point center, float arcRadius, float startDegrees, float endDegrees,
                 float radius)
{
    float span = endDegrees - startDegrees;
    int segments = (int)ceilf(fabsf(span) / ARC_SEGMENT_DEGREES);
    if (segments < 1) {
        segments = 1;
    }
    Point previous = {0, 0};
    for (int i = 0; i <= segments; i++) {
        float angle = (startDegrees + span * (float)i / (float)segments) * PI_FLOAT / 180.0f;
        Point current = {center.x + arcRadius * cosf(angle), center.y + arcRadius * sinf(angle)};
        if (i > 0) {
            mask_add_capsule(mask, previous, current, radius, radius);
        }
        previous = current;
    }
}
