/**
 * @file trail.h
 * @brief Fading pointer trails used by the laser pointer and the eraser.
 *
 * A trail keeps recent timestamped points. Rendering shrinks the width with the age of each point
 * and along the last stretch before the tail, and never changes the colour, as Excalidraw does, so
 * the laser leaves no permanent ink and costs nothing once it has faded.
 */
#ifndef INK_TRAIL_H
#define INK_TRAIL_H

#include <stdbool.h>
#include <stdint.h>

#include "geometry.h"

#define TRAIL_CAPACITY 192

#define TRAIL_LASER_RADIUS 2.0f
#define TRAIL_LASER_LIFETIME_MS 1000
#define TRAIL_ERASER_RADIUS 5.0f
#define TRAIL_ERASER_LIFETIME_MS 200

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
    bool laser;
    float taperLength;
    float smoothing;
    Point smoothed;
} Trail;

/**
 * @brief Configures an empty trail.
 *
 * The laser look uses a 50 pixel taper and streamline 0.4, the eraser look a 10 pixel taper and
 * streamline 0.2, both matching Excalidraw. Excalidraw's own values are TRAIL_LASER_RADIUS with
 * TRAIL_LASER_LIFETIME_MS and TRAIL_ERASER_RADIUS with TRAIL_ERASER_LIFETIME_MS.
 * @param trail Trail to initialize.
 * @param maxRadius Radius at the head of a fresh trail in pixels.
 * @param lifetimeMs Time after which a point has fully faded.
 * @param color Straight-alpha base color.
 * @param laser true for the laser look, false for the eraser look.
 */
void trail_init(Trail *trail, float maxRadius, uint32_t lifetimeMs, Color color, bool laser);

/**
 * @brief Appends a point after streamlining it, dropping the oldest when full.
 * @param trail Trail to extend.
 * @param position Raw point position.
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
