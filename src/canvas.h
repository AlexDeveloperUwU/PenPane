/**
 * @file canvas.h
 * @brief A premultiplied 32-bit pixel surface backed by a GDI DIB section.
 */
#ifndef INK_CANVAS_H
#define INK_CANVAS_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#include "theme.h"

/** @brief Pixel surface that can be handed directly to UpdateLayeredWindow. */
typedef struct Canvas {
    int width;
    int height;
    uint32_t *pixels;
    HBITMAP bitmap;
    HDC dc;
    HGDIOBJ previousBitmap;
} Canvas;

/**
 * @brief Allocates a canvas and clears it to fully transparent.
 * @param canvas Canvas to initialize.
 * @param width Width in pixels, must be positive.
 * @param height Height in pixels, must be positive.
 * @return true on success, false when the DIB section could not be created.
 */
bool canvas_create(Canvas *canvas, int width, int height);

/**
 * @brief Releases every resource owned by the canvas and zeroes it.
 * @param canvas Canvas to release, may already be released.
 */
void canvas_destroy(Canvas *canvas);

/**
 * @brief Fills a rectangle with a premultiplied value, replacing existing pixels.
 * @param canvas Target canvas.
 * @param area Rectangle to fill, clipped to the canvas.
 * @param color Straight-alpha color, converted to premultiplied on write.
 */
void canvas_fill(Canvas *canvas, const RECT *area, Color color);

/**
 * @brief Blends a straight-alpha color through an 8-bit coverage mask using source-over.
 * @param canvas Target canvas.
 * @param originX Canvas x of the mask's left edge.
 * @param originY Canvas y of the mask's top edge.
 * @param coverage Row-major coverage bytes.
 * @param maskWidth Mask width in pixels.
 * @param maskHeight Mask height in pixels.
 * @param color Straight-alpha color.
 */
void canvas_blend_coverage(Canvas *canvas, int originX, int originY, const uint8_t *coverage,
                           int maskWidth, int maskHeight, Color color);

#endif
