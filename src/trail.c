/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trail.h"

#include <math.h>
#include <string.h>

#include "mask_scratch.h"
#include "theme.h"

#define LASER_DOT_RADIUS 5.0f
#define LASER_DOT_GLOW_RADIUS 11.0f
#define TRAIL_FRAME_MS 16.0f
#define TRAIL_LASER_TAPER_LENGTH 50.0f
#define TRAIL_ERASER_TAPER_LENGTH 10.0f
#define TRAIL_LASER_STREAMLINE 0.4f
#define TRAIL_ERASER_STREAMLINE 0.2f

void trail_init(Trail *trail, float maxRadius, uint32_t lifetimeMs, Color color, bool laser)
{
    memset(trail, 0, sizeof *trail);
    trail->maxRadius = maxRadius;
    trail->lifetimeMs = lifetimeMs;
    trail->color = color;
    trail->laser = laser;
    trail->taperLength = laser ? TRAIL_LASER_TAPER_LENGTH : TRAIL_ERASER_TAPER_LENGTH;
    float streamline = laser ? TRAIL_LASER_STREAMLINE : TRAIL_ERASER_STREAMLINE;
    trail->smoothing = 0.15f + (1.0f - streamline) * 0.85f;
}

/**
 * @brief Index of the n-th oldest point.
 * @param trail Trail to index.
 * @param n Position from the tail, 0 is the oldest.
 * @return Ring buffer index.
 */
static int trail_index(const Trail *trail, int n)
{
    return (trail->head + TRAIL_CAPACITY - trail->count + n) % TRAIL_CAPACITY;
}

void trail_add(Trail *trail, Point position, uint32_t nowMs)
{
    trail->raw = position;
    if (trail->count == 0) {
        trail->smoothed = position;
    } else {
        trail->smoothed.x += (position.x - trail->smoothed.x) * trail->smoothing;
        trail->smoothed.y += (position.y - trail->smoothed.y) * trail->smoothing;
    }
    trail->points[trail->head].position = trail->smoothed;
    trail->points[trail->head].timeMs = nowMs;
    trail->head = (trail->head + 1) % TRAIL_CAPACITY;
    if (trail->count < TRAIL_CAPACITY) {
        trail->count++;
    }
}

void trail_settle(Trail *trail, uint32_t nowMs)
{
    for (int i = 0; i < 6 && trail->count > 0 &&
                    hypotf(trail->raw.x - trail->smoothed.x, trail->raw.y - trail->smoothed.y) > 0.5f;
         i++) {
        trail_add(trail, trail->raw, nowMs);
    }
}

bool trail_prune(Trail *trail, uint32_t nowMs)
{
    while (trail->count > 0) {
        const TrailPoint *oldest = &trail->points[trail_index(trail, 0)];
        if (nowMs - oldest->timeMs < trail->lifetimeMs) {
            break;
        }
        trail->count--;
    }
    return trail->count > 0;
}

void trail_clear(Trail *trail)
{
    trail->count = 0;
}

RECT trail_bounds(const Trail *trail)
{
    RECT bounds = {0, 0, 0, 0};
    if (trail->count == 0) {
        return bounds;
    }
    float reach = trail->maxRadius + 2.0f;
    FloatRect box = {1e9f, 1e9f, -1e9f, -1e9f};
    for (int i = 0; i < trail->count; i++) {
        Point p = trail->points[trail_index(trail, i)].position;
        box.left = p.x < box.left ? p.x : box.left;
        box.right = p.x > box.right ? p.x : box.right;
        box.top = p.y < box.top ? p.y : box.top;
        box.bottom = p.y > box.bottom ? p.y : box.bottom;
    }
    return rect_from_float(box, reach);
}

/**
 * @brief Quartic ease-out used for the width mapping of Excalidraw's trails.
 * @param t Value in 0..1.
 * @return Eased value in 0..1.
 */
static float ease_out(float t)
{
    float inverse = 1.0f - t;
    inverse *= inverse;
    return 1.0f - inverse * inverse;
}

/**
 * @brief Width factor of one point from its age and its distance to the head.
 * @param trail Trail being drawn.
 * @param ageMs Age of the point in milliseconds.
 * @param distanceToHead Time between the point and the newest point expressed in frames of 16 milliseconds.
 * @return Factor in 0..1 applied to the maximum radius.
 */
static float width_factor(const Trail *trail, uint32_t ageMs, float distanceToHead)
{
    float life = 1.0f - (float)ageMs / (float)trail->lifetimeMs;
    life = life < 0.0f ? 0.0f : life;
    float taper = (trail->taperLength - fminf(trail->taperLength, distanceToHead)) / trail->taperLength;
    return fminf(life, ease_out(taper));
}

RECT laser_dot_bounds(Point center)
{
    FloatRect box = {center.x, center.y, center.x, center.y};
    return rect_from_float(box, LASER_DOT_GLOW_RADIUS + 2.0f);
}

void laser_dot_render(Canvas *canvas, const RECT *clip, Point center)
{
    RECT bounds = laser_dot_bounds(center);
    RECT area;
    if (!rect_intersect(bounds, *clip, &area)) {
        return;
    }
    Mask glow;
    if (mask_scratch_acquire(&glow, area)) {
        mask_add_ellipse_fill(&glow, center, LASER_DOT_GLOW_RADIUS, LASER_DOT_GLOW_RADIUS);
        mask_blend(canvas, &glow, (COLOR_LASER & 0x00FFFFFFu) | 0x55000000u);
    }
    Mask core;
    if (mask_scratch_acquire(&core, area)) {
        mask_add_ellipse_fill(&core, center, LASER_DOT_RADIUS, LASER_DOT_RADIUS);
        mask_blend(canvas, &core, COLOR_LASER);
    }
}

void trail_render(const Trail *trail, Canvas *canvas, const RECT *clip, uint32_t nowMs)
{
    RECT bounds = trail_bounds(trail);
    RECT area;
    if (trail->count < 1 || !rect_intersect(bounds, *clip, &area)) {
        return;
    }
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    const TrailPoint *newest = &trail->points[trail_index(trail, trail->count - 1)];
    const TrailPoint *newer = newest;
    float radiusNewer = trail->maxRadius * width_factor(trail, nowMs - newer->timeMs, 0.0f);
    if (trail->count == 1) {
        mask_add_capsule(&mask, newest->position, newest->position, radiusNewer, radiusNewer);
    }
    for (int i = trail->count - 2; i >= 0; i--) {
        const TrailPoint *older = &trail->points[trail_index(trail, i)];
        float distance = (float)(newest->timeMs - older->timeMs) / TRAIL_FRAME_MS;
        float radiusOlder = trail->maxRadius * width_factor(trail, nowMs - older->timeMs, distance);
        mask_add_capsule(&mask, older->position, newer->position, radiusOlder, radiusNewer);
        newer = older;
        radiusNewer = radiusOlder;
    }
    mask_blend(canvas, &mask, trail->color);
}
