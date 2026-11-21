/**
 * @file render.h
 * @brief Draws elements and scenes onto a canvas, restricted to a clip rectangle.
 */
#ifndef INK_RENDER_H
#define INK_RENDER_H

#include "canvas.h"
#include "element.h"
#include "geometry.h"
#include "scene.h"

/**
 * @brief Draws one element, clipped.
 * @param canvas Target canvas.
 * @param element Element to draw.
 * @param clip Pixel rectangle to restrict drawing to.
 */
void render_element(Canvas *canvas, const Element *element, const RECT *clip);

/**
 * @brief Clears a region to a backdrop color and draws every element that intersects it.
 * @param canvas Target canvas.
 * @param scene Scene to draw.
 * @param clip Pixel rectangle to repaint.
 * @param backdrop Backdrop color: 0 transparent, 0x01000000 for hit-testable transparency, or an opaque color.
 */
void render_scene(Canvas *canvas, const Scene *scene, const RECT *clip, Color backdrop);

/**
 * @brief Draws the newest part of a pen stroke directly on the canvas without clearing.
 * @param canvas Target canvas.
 * @param element Pen element being drawn.
 * @param firstGroup First point group that changed.
 * @param clip Pixel rectangle to restrict drawing to.
 * @return Pixel bounds that were touched.
 */
RECT render_pen_tail(Canvas *canvas, const Element *element, int firstGroup, const RECT *clip);

/**
 * @brief Draws selection outline and resize handles around an element.
 * @param canvas Target canvas.
 * @param element Selected element.
 * @param clip Pixel rectangle to restrict drawing to.
 */
void render_selection(Canvas *canvas, const Element *element, const RECT *clip);

/**
 * @brief Padded box that the selection outline and handles are drawn around.
 * @param element Selected element.
 * @return Floating rectangle; handle centers sit on its corners and edge midpoints.
 */
FloatRect render_selection_box(const Element *element);

/**
 * @brief Pixel bounds of the selection decoration for an element.
 * @param element Selected element.
 * @return Rectangle containing outline and handles.
 */
RECT render_selection_bounds(const Element *element);

/**
 * @brief Draws a text caret.
 * @param canvas Target canvas.
 * @param element Text element being edited.
 * @param clip Pixel rectangle to restrict drawing to.
 */
void render_text_caret(Canvas *canvas, const Element *element, const RECT *clip);

#endif
