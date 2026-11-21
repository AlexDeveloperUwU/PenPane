#include "trail.h"

#include <math.h>
#include <string.h>

#include "mask_scratch.h"

#define TRAIL_GLOW_SCALE 2.6f
#define TRAIL_GLOW_ALPHA 0.22f

void trail_init(Trail *trail, float maxRadius, uint32_t lifetimeMs, Color color, bool glow)
{
    memset(trail, 0, sizeof *trail);
    trail->maxRadius = maxRadius;
    trail->lifetimeMs = lifetimeMs;
    trail->color = color;
    trail->glow = glow;
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
    trail->points[trail->head].position = position;
    trail->points[trail->head].timeMs = nowMs;
    trail->head = (trail->head + 1) % TRAIL_CAPACITY;
    if (trail->count < TRAIL_CAPACITY) {
        trail->count++;
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
    float reach = trail->maxRadius * (trail->glow ? TRAIL_GLOW_SCALE : 1.0f) + 2.0f;
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
 * @brief Quadratic ease-out used for width and fade, as in Excalidraw's trails.
 * @param t Value in 0..1.
 * @return Eased value in 0..1.
 */
static float ease_out(float t)
{
    return 1.0f - (1.0f - t) * (1.0f - t);
}

/**
 * @brief Draws one pass of the trail with a radius scale and an alpha multiplier.
 * @param trail Trail to draw.
 * @param canvas Target canvas.
 * @param area Pixel rectangle of work.
 * @param nowMs Clock used for fading.
 * @param scale Radius multiplier.
 * @param alpha Alpha multiplier applied to the base color.
 */
static void render_pass(const Trail *trail, Canvas *canvas, RECT area, uint32_t nowMs, float scale,
                        float alpha)
{
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    for (int i = 0; i + 1 < trail->count; i++) {
        const TrailPoint *a = &trail->points[trail_index(trail, i)];
        const TrailPoint *b = &trail->points[trail_index(trail, i + 1)];
        float ageA = (float)(nowMs - a->timeMs) / (float)trail->lifetimeMs;
        float ageB = (float)(nowMs - b->timeMs) / (float)trail->lifetimeMs;
        ageA = ageA > 1.0f ? 1.0f : ageA;
        ageB = ageB > 1.0f ? 1.0f : ageB;
        float radiusA = trail->maxRadius * scale * ease_out(1.0f - ageA);
        float radiusB = trail->maxRadius * scale * ease_out(1.0f - ageB);
        mask_add_capsule(&mask, a->position, b->position, radiusA, radiusB);
    }
    Color color = trail->color;
    uint32_t base = color >> 24;
    uint32_t scaled = (uint32_t)((float)base * alpha);
    mask_blend(canvas, &mask, (color & 0x00FFFFFFu) | (scaled << 24));
}

void trail_render(const Trail *trail, Canvas *canvas, const RECT *clip, uint32_t nowMs)
{
    RECT bounds = trail_bounds(trail);
    RECT area;
    if (trail->count < 2 || !rect_intersect(bounds, *clip, &area)) {
        return;
    }
    if (trail->glow) {
        render_pass(trail, canvas, area, nowMs, TRAIL_GLOW_SCALE, TRAIL_GLOW_ALPHA);
    }
    render_pass(trail, canvas, area, nowMs, 1.0f, 1.0f);
}
