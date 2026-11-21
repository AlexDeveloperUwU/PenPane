/**
 * @file pen.h
 * @brief Freehand stroke model: streamlining, simulated pressure and variable-width outline emission.
 *
 * Follows Excalidraw's perfect-freehand settings: thinning 0.6, streamline 0.5, size 4.25 times the
 * stroke width, sine easing, and speed based pressure simulation when no real pressure exists.
 */
#ifndef INK_PEN_H
#define INK_PEN_H

#include <stdbool.h>

#include "element.h"

/** @brief Running state while a stroke is being captured. */
typedef struct PenBuilder {
    Point smoothed;
    Point lastRaw;
    float pressure;
    bool started;
} PenBuilder;

/**
 * @brief Receives one rounded segment of a stroke outline.
 * @param user Opaque pointer given to pen_emit.
 * @param from Segment start.
 * @param to Segment end.
 * @param radiusFrom Radius at the start.
 * @param radiusTo Radius at the end.
 */
typedef void (*PenSegmentSink)(void *user, Point from, Point to, float radiusFrom, float radiusTo);

/**
 * @brief Starts a stroke at a position.
 * @param builder Builder to reset.
 * @param element Pen element receiving points.
 * @param position First raw position.
 * @param pressure Real pressure in 0..1, or a negative value to simulate it.
 * @return true on success.
 */
bool pen_begin(PenBuilder *builder, Element *element, Point position, float pressure);

/**
 * @brief Adds a raw sample, applying streamlining and pressure.
 * @param builder Builder from pen_begin.
 * @param element Pen element receiving points.
 * @param position Raw position.
 * @param pressure Real pressure in 0..1, or a negative value to simulate it.
 * @return true when a point was added.
 */
bool pen_add(PenBuilder *builder, Element *element, Point position, float pressure);

/**
 * @brief Radius of a stroke at a pressure.
 * @param width Stroke width preset in pixels.
 * @param pressure Pressure in 0..1.
 * @return Radius in pixels.
 */
float pen_radius(float width, float pressure);

/**
 * @brief Emits the segments of a stroke starting at a point group.
 * @param element Pen element.
 * @param firstGroup Index of the first point group to emit; 0 emits the whole stroke.
 * @param sink Receives segments.
 * @param user Opaque pointer forwarded to the sink.
 */
void pen_emit(const Element *element, int firstGroup, PenSegmentSink sink, void *user);

#endif
