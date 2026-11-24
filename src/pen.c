/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pen.h"

#include <math.h>

#define PEN_SIZE_FACTOR 4.25f
#define PEN_THINNING 0.6f
#define PEN_STREAMLINE 0.5f
#define PEN_PRESSURE_RATE 0.275f
#define PEN_START_PRESSURE 0.25f
#define PEN_CONSTANT_STORED_PRESSURE 0.0158f
#define PEN_SPEED_SIZE_FACTOR 2.125f
#define PEN_MAX_STEPS 16
#define PEN_STEP_LENGTH 2.5f
#define PEN_DOT_PRESSURE 0.5f
#define PEN_CONSTANT_DOT_LIMIT 0.05f
#define PEN_MIN_DISTANCE 0.5f
#define PI_FLOAT 3.14159265f

float pen_radius(float width, float pressure)
{
    float size = width * PEN_SIZE_FACTOR;
    float t = 0.5f - PEN_THINNING * (0.5f - pressure);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return size * sinf(t * PI_FLOAT * 0.5f) * 0.5f;
}

/**
 * @brief Streamlining blend factor of perfect-freehand for the configured streamline value.
 * @return Fraction of the distance to the raw sample that the smoothed point moves per sample.
 */
static float streamline_factor(void)
{
    return 0.15f + (1.0f - PEN_STREAMLINE) * 0.85f;
}

bool pen_begin(PenBuilder *builder, Element *element, Point position, float pressure)
{
    builder->smoothed = position;
    builder->lastRaw = position;
    builder->constantWidth = pressure < PEN_PRESSURE_SIMULATED - 0.5f;
    if (builder->constantWidth) {
        builder->pressure = PEN_CONSTANT_STORED_PRESSURE;
    } else {
        builder->pressure = pressure >= 0.0f ? pressure : PEN_START_PRESSURE;
    }
    builder->started = true;
    PenPoint point = {position.x, position.y, builder->pressure};
    return element_add_point(element, point);
}

bool pen_add(PenBuilder *builder, Element *element, Point position, float pressure)
{
    float dx = position.x - builder->lastRaw.x;
    float dy = position.y - builder->lastRaw.y;
    if (sqrtf(dx * dx + dy * dy) < PEN_MIN_DISTANCE) {
        return false;
    }
    builder->lastRaw = position;
    Point previous = builder->smoothed;
    float blend = streamline_factor();
    builder->smoothed.x += (position.x - builder->smoothed.x) * blend;
    builder->smoothed.y += (position.y - builder->smoothed.y) * blend;
    if (builder->constantWidth) {
        builder->pressure = PEN_CONSTANT_STORED_PRESSURE;
    } else if (pressure >= 0.0f) {
        builder->pressure = pressure;
    } else {
        float stepX = builder->smoothed.x - previous.x;
        float stepY = builder->smoothed.y - previous.y;
        float size = element->style.width * PEN_SPEED_SIZE_FACTOR;
        float speed = sqrtf(stepX * stepX + stepY * stepY) / size;
        speed = speed > 1.0f ? 1.0f : speed;
        float target = 1.0f - speed;
        builder->pressure += (target - builder->pressure) * (speed * PEN_PRESSURE_RATE);
        builder->pressure = builder->pressure > 1.0f ? 1.0f : builder->pressure;
    }
    PenPoint point = {builder->smoothed.x, builder->smoothed.y, builder->pressure};
    return element_add_point(element, point);
}

bool pen_finish(PenBuilder *builder, Element *element)
{
    if (!builder->started || element->pointCount == 0) {
        return false;
    }
    const PenPoint *last = &element->points[element->pointCount - 1];
    if (fabsf(last->x - builder->lastRaw.x) < 0.05f && fabsf(last->y - builder->lastRaw.y) < 0.05f) {
        return false;
    }
    PenPoint point = {builder->lastRaw.x, builder->lastRaw.y, builder->pressure};
    builder->smoothed = builder->lastRaw;
    return element_add_point(element, point);
}

/**
 * @brief Midpoint between two pen points, with averaged pressure.
 * @param a First point.
 * @param b Second point.
 * @return Midpoint.
 */
static PenPoint midpoint(PenPoint a, PenPoint b)
{
    PenPoint mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.pressure + b.pressure) * 0.5f};
    return mid;
}

/**
 * @brief Emits a quadratic curve as short capsule segments.
 * @param start Curve start with pressure.
 * @param control Control point with pressure.
 * @param end Curve end with pressure.
 * @param width Stroke width preset.
 * @param sink Segment receiver.
 * @param user Receiver context.
 */
static void emit_curve(PenPoint start, PenPoint control, PenPoint end, float width, PenSegmentSink sink,
                       void *user)
{
    float chord = hypotf(end.x - start.x, end.y - start.y) + hypotf(control.x - start.x, control.y - start.y);
    int steps = (int)ceilf(chord / PEN_STEP_LENGTH);
    steps = steps < 1 ? 1 : (steps > PEN_MAX_STEPS ? PEN_MAX_STEPS : steps);
    Point previous = {start.x, start.y};
    float previousRadius = pen_radius(width, start.pressure);
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        Point current = {u * u * start.x + 2.0f * u * t * control.x + t * t * end.x,
                         u * u * start.y + 2.0f * u * t * control.y + t * t * end.y};
        float pressure = u * u * start.pressure + 2.0f * u * t * control.pressure + t * t * end.pressure;
        float radius = pen_radius(width, pressure);
        sink(user, previous, current, previousRadius, radius);
        previous = current;
        previousRadius = radius;
    }
}

void pen_emit(const Element *element, int firstGroup, PenSegmentSink sink, void *user)
{
    int count = element->pointCount;
    float width = element->style.width;
    if (count == 0) {
        return;
    }
    const PenPoint *p = element->points;
    if (count == 1) {
        Point at = {p[0].x, p[0].y};
        float stored = p[0].pressure;
        float radius = pen_radius(
            width, stored < PEN_CONSTANT_DOT_LIMIT || stored > PEN_DOT_PRESSURE ? stored : PEN_DOT_PRESSURE);
        sink(user, at, at, radius, radius);
        return;
    }
    for (int group = firstGroup < 0 ? 0 : firstGroup; group < count; group++) {
        if (group == 0) {
            PenPoint end = midpoint(p[0], p[1]);
            emit_curve(p[0], midpoint(p[0], end), end, width, sink, user);
        } else if (group == count - 1) {
            PenPoint start = midpoint(p[count - 2], p[count - 1]);
            emit_curve(start, midpoint(start, p[count - 1]), p[count - 1], width, sink, user);
        } else {
            emit_curve(midpoint(p[group - 1], p[group]), p[group], midpoint(p[group], p[group + 1]), width,
                       sink, user);
        }
    }
}
