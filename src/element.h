/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file element.h
 * @brief Editable drawing objects. Everything on screen is stored as elements, never as baked pixels.
 */
#ifndef PENPANE_ELEMENT_H
#define PENPANE_ELEMENT_H

#include <stdbool.h>
#include <wchar.h>

#include "geometry.h"
#include "style.h"

/** @brief Kinds of element. */
typedef enum ElementType {
    ELEMENT_RECTANGLE,
    ELEMENT_ELLIPSE,
    ELEMENT_DIAMOND,
    ELEMENT_ARROW,
    ELEMENT_LINE,
    ELEMENT_PEN,
    ELEMENT_TEXT
} ElementType;

/** @brief One sampled pen point with its normalized pressure. */
typedef struct PenPoint {
    float x;
    float y;
    float pressure;
} PenPoint;

/**
 * @brief A drawing element.
 *
 * Rectangles, ellipses and diamonds use from and to as opposite corners of their box. Lines and arrows use
 * them as the two endpoints. Text uses from as the top-left origin. Pen strokes use the points array.
 */
typedef struct Element {
    ElementType type;
    Style style;
    Point from;
    Point to;
    PenPoint *points;
    int pointCount;
    int pointCapacity;
    wchar_t *text;
    int fontSize;
    bool erasing;
} Element;

/**
 * @brief Creates an empty element of a type with a style.
 * @param type Element kind.
 * @param style Appearance.
 * @return Initialized element owning no memory yet.
 */
Element element_make(ElementType type, const Style *style);

/**
 * @brief Deep copies an element.
 * @param destination Receives the copy; previous contents are overwritten, not freed.
 * @param source Element to copy.
 * @return true on success, false when memory ran out (destination is left empty).
 */
bool element_copy(Element *destination, const Element *source);

/**
 * @brief Releases memory owned by an element and zeroes it.
 * @param element Element to release.
 */
void element_free(Element *element);

/**
 * @brief Appends a pen point, growing storage as needed.
 * @param element Pen element.
 * @param point Point to append.
 * @return true on success.
 */
bool element_add_point(Element *element, PenPoint point);

/**
 * @brief Replaces the text of a text element.
 * @param element Text element.
 * @param text New content, copied.
 * @return true on success.
 */
bool element_set_text(Element *element, const wchar_t *text);

/**
 * @brief Geometric bounds without stroke margins.
 * @param element Element to measure.
 * @return Tight bounding box of the element's geometry.
 */
FloatRect element_box(const Element *element);

/** @brief Length of an arrowhead's sides, in pixels. */
#define ARROWHEAD_SIZE 25.0f

/** @brief Side of a resize handle, in pixels. */
#define ELEMENT_HANDLE_SIZE 8.0f

/**
 * @brief Padded box that the selection outline and handles are drawn around.
 * @param element Selected element.
 * @return Floating rectangle; handle centers sit on its corners and edge midpoints.
 */
FloatRect element_selection_box(const Element *element);

/**
 * @brief Pixel bounds of the selection decoration for an element.
 * @param element Selected element.
 * @return Rectangle containing outline and handles.
 */
RECT element_selection_bounds(const Element *element);

/**
 * @brief Pixel bounds including stroke width, arrowhead and antialiasing margins.
 * @param element Element to measure.
 * @return Integer rectangle that contains every pixel the element can touch.
 */
RECT element_bounds(const Element *element);

/**
 * @brief Moves an element.
 * @param element Element to move.
 * @param dx Horizontal offset in pixels.
 * @param dy Vertical offset in pixels.
 */
void element_translate(Element *element, float dx, float dy);

/**
 * @brief Maps an element's geometry from one box onto another (resize).
 * @param element Element to transform.
 * @param oldBox Box before resizing.
 * @param newBox Box after resizing.
 */
void element_remap(Element *element, FloatRect oldBox, FloatRect newBox);

/**
 * @brief Reports whether a rectangle or ellipse has a visible fill.
 * @param element Element to test.
 * @return true when the element is closed and filled.
 */
bool element_is_filled(const Element *element);

#endif
