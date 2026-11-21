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
    TEXT_FONT_CANVAS
} TextFont;

/**
 * @brief Measures text, supporting multiple lines separated by '\n'.
 * @param font Font role: UI uses Segoe UI, canvas uses a handwriting-friendly sans font available on Windows.
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
 * @brief Releases cached GDI fonts and scratch surfaces.
 */
void text_shutdown(void);

#endif
