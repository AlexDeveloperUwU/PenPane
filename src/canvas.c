/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "canvas.h"

#include <string.h>

/**
 * @brief Converts a straight-alpha color to a premultiplied pixel value.
 * @param color Straight-alpha color.
 * @return Premultiplied 0xAARRGGBB pixel.
 */
static uint32_t premultiply(Color color)
{
    uint32_t alpha = color >> 24;
    uint32_t red = ((color >> 16) & 0xFF) * alpha / 255;
    uint32_t green = ((color >> 8) & 0xFF) * alpha / 255;
    uint32_t blue = (color & 0xFF) * alpha / 255;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

bool canvas_create(Canvas *canvas, int width, int height)
{
    memset(canvas, 0, sizeof *canvas);
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    canvas->bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!canvas->bitmap) {
        return false;
    }
    canvas->dc = CreateCompatibleDC(NULL);
    if (!canvas->dc) {
        DeleteObject(canvas->bitmap);
        memset(canvas, 0, sizeof *canvas);
        return false;
    }
    canvas->previousBitmap = SelectObject(canvas->dc, canvas->bitmap);
    canvas->pixels = bits;
    canvas->width = width;
    canvas->height = height;
    return true;
}

void canvas_destroy(Canvas *canvas)
{
    if (canvas->dc) {
        SelectObject(canvas->dc, canvas->previousBitmap);
        DeleteDC(canvas->dc);
    }
    if (canvas->bitmap) {
        DeleteObject(canvas->bitmap);
    }
    memset(canvas, 0, sizeof *canvas);
}

void canvas_fill(Canvas *canvas, const RECT *area, Color color)
{
    int left = area->left - canvas->originX;
    int top = area->top - canvas->originY;
    int right = area->right - canvas->originX;
    int bottom = area->bottom - canvas->originY;
    left = left < 0 ? 0 : left;
    top = top < 0 ? 0 : top;
    right = right > canvas->width ? canvas->width : right;
    bottom = bottom > canvas->height ? canvas->height : bottom;
    uint32_t value = premultiply(color);
    for (int y = top; y < bottom; y++) {
        uint32_t *row = canvas->pixels + (size_t)y * canvas->width;
        for (int x = left; x < right; x++) {
            row[x] = value;
        }
    }
}

void canvas_blend_coverage(Canvas *canvas, int originX, int originY, const uint8_t *coverage, int maskWidth,
                           int maskHeight, Color color)
{
    originX -= canvas->originX;
    originY -= canvas->originY;
    uint32_t colorAlpha = color >> 24;
    uint32_t red = (color >> 16) & 0xFF;
    uint32_t green = (color >> 8) & 0xFF;
    uint32_t blue = color & 0xFF;
    uint32_t solid = 0xFF000000u | (red << 16) | (green << 8) | blue;
    int first = originX < 0 ? -originX : 0;
    int last = originX + maskWidth > canvas->width ? canvas->width - originX : maskWidth;

    for (int my = 0; my < maskHeight; my++) {
        int y = originY + my;
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        uint32_t *row = canvas->pixels + (size_t)y * canvas->width + originX;
        const uint8_t *maskRow = coverage + (size_t)my * maskWidth;
        for (int mx = first; mx < last; mx++) {
            if (mx + 8 <= last) {
                uint64_t group;
                memcpy(&group, maskRow + mx, sizeof group);
                if (group == 0) {
                    mx += 7;
                    continue;
                }
            }
            uint32_t cover = maskRow[mx];
            if (cover == 0) {
                continue;
            }
            if (cover == 255 && colorAlpha == 255) {
                row[mx] = solid;
                continue;
            }
            uint32_t srcAlpha = cover * colorAlpha / 255;
            uint32_t inverse = 255 - srcAlpha;
            uint32_t dst = row[mx];
            uint32_t outAlpha = srcAlpha + (dst >> 24) * inverse / 255;
            uint32_t outRed = (red * srcAlpha + ((dst >> 16) & 0xFF) * inverse) / 255;
            uint32_t outGreen = (green * srcAlpha + ((dst >> 8) & 0xFF) * inverse) / 255;
            uint32_t outBlue = (blue * srcAlpha + (dst & 0xFF) * inverse) / 255;
            row[mx] = (outAlpha << 24) | (outRed << 16) | (outGreen << 8) | outBlue;
        }
    }
}

RECT canvas_area(const Canvas *canvas)
{
    RECT area = {canvas->originX, canvas->originY, canvas->originX + canvas->width,
                 canvas->originY + canvas->height};
    return area;
}
