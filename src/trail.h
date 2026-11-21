/**
 * @file trail.h
 * @brief Fading pointer trails used by the laser pointer and the eraser.
 *
 * A trail keeps recent timestamped points. Rendering tapers the width toward the tail and fades
 * with age, so the laser leaves no permanent ink and costs nothing once it has faded.
 */
#ifndef INK_TRAIL_H
#define INK_TRAIL_H

#include <stdbool.h>
#include <stdint.h>

#include "geometry.h"

#define TRAIL_CAPACITY 192

/** @brief One timestamped trail point. */
typedef struct TrailPoint {
    Point position;
    uint32_t timeMs;
} TrailPoint;

/** @brief Ring of recent points plus its look. */
typedef struct Trail {
    TrailPoint points[TRAIL_CAPACITY];
    int head;
    int count;
    float maxRadius;
    uint32_t lifetimeMs;
    Color color;
    bool glow;
} Trail;

/**
 * @brief Configures an empty trail.
 * @param trail Trail to initialize.
 * @param maxRadius Radius at the head of a fresh trail in pixels.
 * @param lifetimeMs Time after which a point has fully faded.
 * @param color Straight-alpha base color.
 * @param glow true to draw a soft halo under the core.
 */
void trail_init(Trail *trail, float maxRadius, uint32_t lifetimeMs, Color color, bool glow);

/**
 * @brief Appends a point, dropping the oldest when full.
 * @param trail Trail to extend.
 * @param position Point position.
 * @param nowMs Monotonic millisecond clock.
 */
void trail_add(Trail *trail, Point position, uint32_t nowMs);

/**
 * @brief Drops points that have fully faded.
 * @param trail Trail to prune.
 * @param nowMs Monotonic millisecond clock.
 * @return true while the trail still has visible points and needs animation frames.
 */
bool trail_prune(Trail *trail, uint32_t nowMs);

/**
 * @brief Removes every point.
 * @param trail Trail to empty.
 */
void trail_clear(Trail *trail);

/**
 * @brief Pixel bounds of all points including the stroke radius.
 * @param trail Trail to measure.
 * @return Bounding rectangle, empty when the trail has no points.
 */
RECT trail_bounds(const Trail *trail);

/**
 * @brief Draws the trail onto a canvas, clipped to a rectangle.
 * @param trail Trail to draw.
 * @param canvas Target canvas.
 * @param clip Pixel rectangle to restrict drawing to.
 * @param nowMs Monotonic millisecond clock used for fading.
 */
void trail_render(const Trail *trail, Canvas *canvas, const RECT *clip, uint32_t nowMs);

#endif
