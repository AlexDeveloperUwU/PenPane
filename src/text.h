/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file text.h
 * @brief Text measurement and rasterization into coverage masks using GDI with grayscale anti-aliasing.
 */
#ifndef PENPANE_TEXT_H
#define PENPANE_TEXT_H

#include <stdbool.h>
#include <wchar.h>

#include "raster.h"

/** @brief Font roles. */
typedef enum TextFont {
    TEXT_FONT_UI,
    TEXT_FONT_CANVAS,
    TEXT_FONT_CANVAS_NORMAL,
    TEXT_FONT_CANVAS_LEGIBLE
} TextFont;

/**
 * @brief Maps a TextFace value of a style to the canvas font role that draws it.
 * @param face Value of the TextFace enum; out-of-range values give the handwritten role.
 * @return Font role to pass to the measuring and drawing functions.
 */
TextFont text_font_for_face(int face);

/**
 * @brief Measures text, supporting multiple lines separated by '\n'.
 * @param font Font role: UI uses Segoe UI; canvas uses a handwriting font when installed, the normal role
 *        Bahnschrift (else Segoe UI) and the legible role Cascadia Mono (else Consolas, then Segoe UI).
 * @param text Text to measure.
 * @param pixelSize Font height in pixels.
 * @param width Receives the width in pixels.
 * @param height Receives the height in pixels.
 */
void text_measure(TextFont font, const wchar_t *text, int pixelSize, int *width, int *height);

/**
 * @brief Rasterizes text into a new mask positioned at the given top-left origin.
 * @param mask Receives a newly allocated mask; release with mask_destroy.
 * @param font Font role.
 * @param text Text to draw, lines separated by '\n'.
 * @param pixelSize Font height in pixels.
 * @param x Canvas x of the text's left edge.
 * @param y Canvas y of the text's top edge.
 * @return true on success, false for empty text or allocation failure.
 */
bool text_make_mask(Mask *mask, TextFont font, const wchar_t *text, int pixelSize, int x, int y);

/**
 * @brief Rasterizes text like text_make_mask, aligning each line inside the width of the widest one.
 * @param mask Receives a newly allocated mask; release with mask_destroy.
 * @param font Font role.
 * @param text Text to draw, lines separated by '\n'.
 * @param pixelSize Font height in pixels.
 * @param x Canvas x of the text block's left edge.
 * @param y Canvas y of the text block's top edge.
 * @param align Value of the TextAlign enum of style.h; out-of-range values align left.
 * @return true on success, false for empty text or allocation failure.
 */
bool text_make_mask_aligned(Mask *mask, TextFont font, const wchar_t *text, int pixelSize, int x, int y,
                            int align);

/**
 * @brief Gives the horizontal offset of a line inside a text block.
 * @param blockWidth Width of the widest line.
 * @param lineWidth Width of the line.
 * @param align Value of the TextAlign enum of style.h; out-of-range values align left.
 * @return Offset in pixels from the block's left edge to the line's left edge.
 */
int text_line_offset(int blockWidth, int lineWidth, int align);

/**
 * @brief Releases cached GDI fonts and scratch surfaces.
 */
void text_shutdown(void);

#endif
