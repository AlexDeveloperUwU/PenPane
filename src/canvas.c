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
    int left = area->left < 0 ? 0 : area->left;
    int top = area->top < 0 ? 0 : area->top;
    int right = area->right > canvas->width ? canvas->width : area->right;
    int bottom = area->bottom > canvas->height ? canvas->height : area->bottom;
    uint32_t value = premultiply(color);
    for (int y = top; y < bottom; y++) {
        uint32_t *row = canvas->pixels + (size_t)y * canvas->width;
        for (int x = left; x < right; x++) {
            row[x] = value;
        }
    }
}

void canvas_blend_coverage(Canvas *canvas, int originX, int originY, const uint8_t *coverage,
                           int maskWidth, int maskHeight, Color color)
{
    uint32_t colorAlpha = color >> 24;
    uint32_t red = (color >> 16) & 0xFF;
    uint32_t green = (color >> 8) & 0xFF;
    uint32_t blue = color & 0xFF;

    for (int my = 0; my < maskHeight; my++) {
        int y = originY + my;
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        uint32_t *row = canvas->pixels + (size_t)y * canvas->width;
        const uint8_t *maskRow = coverage + (size_t)my * maskWidth;
        for (int mx = 0; mx < maskWidth; mx++) {
            int x = originX + mx;
            uint32_t cover = maskRow[mx];
            if (cover == 0 || x < 0 || x >= canvas->width) {
                continue;
            }
            uint32_t srcAlpha = cover * colorAlpha / 255;
            uint32_t inverse = 255 - srcAlpha;
            uint32_t dst = row[x];
            uint32_t outAlpha = srcAlpha + (dst >> 24) * inverse / 255;
            uint32_t outRed = (red * srcAlpha + ((dst >> 16) & 0xFF) * inverse) / 255;
            uint32_t outGreen = (green * srcAlpha + ((dst >> 8) & 0xFF) * inverse) / 255;
            uint32_t outBlue = (blue * srcAlpha + (dst & 0xFF) * inverse) / 255;
            row[x] = (outAlpha << 24) | (outRed << 16) | (outGreen << 8) | outBlue;
        }
    }
}
