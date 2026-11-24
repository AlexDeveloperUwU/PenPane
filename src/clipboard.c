/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "clipboard.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"

/**
 * @brief Allocates global memory holding a copy of data.
 * @param data Bytes to copy.
 * @param size Number of bytes.
 * @return Movable global handle or NULL.
 */
static HGLOBAL global_copy(const void *data, size_t size)
{
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!handle) {
        return NULL;
    }
    void *target = GlobalLock(handle);
    if (!target) {
        GlobalFree(handle);
        return NULL;
    }
    memcpy(target, data, size);
    GlobalUnlock(handle);
    return handle;
}

/**
 * @brief Hands a global memory block to the open clipboard, releasing it when the clipboard refuses it.
 * @param format Clipboard format.
 * @param handle Block to hand over, may be NULL.
 * @return true when the clipboard took ownership of the block.
 */
static bool clipboard_put(UINT format, HGLOBAL handle)
{
    if (!handle) {
        return false;
    }
    if (!SetClipboardData(format, handle)) {
        GlobalFree(handle);
        return false;
    }
    return true;
}

/**
 * @brief Converts a canvas region to straight-alpha RGBA rows.
 * @param canvas Source canvas holding premultiplied pixels.
 * @param region Region to convert; clipped to the canvas.
 * @param opaqueBackdrop true to force full alpha.
 * @param outWidth Receives the width of the clipped region.
 * @param outHeight Receives the height of the clipped region.
 * @return Newly allocated rows, or NULL when the region is empty or memory ran out.
 */
static uint8_t *region_to_rgba(const Canvas *canvas, RECT region, bool opaqueBackdrop, int *outWidth,
                               int *outHeight)
{
    if (region.left < 0)
        region.left = 0;
    if (region.top < 0)
        region.top = 0;
    if (region.right > canvas->width)
        region.right = canvas->width;
    if (region.bottom > canvas->height)
        region.bottom = canvas->height;
    int width = region.right - region.left;
    int height = region.bottom - region.top;
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    uint8_t *rgba = malloc((size_t)width * (size_t)height * 4);
    if (!rgba) {
        return NULL;
    }
    for (int y = 0; y < height; y++) {
        const uint32_t *row = canvas->pixels + (size_t)(region.top + y) * canvas->width + region.left;
        uint8_t *out = rgba + (size_t)y * width * 4;
        for (int x = 0; x < width; x++) {
            uint32_t pixel = row[x];
            uint32_t alpha = opaqueBackdrop ? 255 : pixel >> 24;
            uint32_t red = (pixel >> 16) & 0xFF;
            uint32_t green = (pixel >> 8) & 0xFF;
            uint32_t blue = pixel & 0xFF;
            if (!opaqueBackdrop && alpha > 0 && alpha < 255) {
                red = red * 255 / alpha;
                green = green * 255 / alpha;
                blue = blue * 255 / alpha;
            }
            out[x * 4] = (uint8_t)(red > 255 ? 255 : red);
            out[x * 4 + 1] = (uint8_t)(green > 255 ? 255 : green);
            out[x * 4 + 2] = (uint8_t)(blue > 255 ? 255 : blue);
            out[x * 4 + 3] = (uint8_t)alpha;
        }
    }
    *outWidth = width;
    *outHeight = height;
    return rgba;
}

bool clipboard_save_region(const Canvas *canvas, RECT region, bool opaqueBackdrop, const wchar_t *path)
{
    int width;
    int height;
    uint8_t *rgba = region_to_rgba(canvas, region, opaqueBackdrop, &width, &height);
    if (!rgba) {
        return false;
    }
    size_t pngSize = 0;
    uint8_t *png = png_encode_rgba(rgba, width, height, &pngSize);
    free(rgba);
    if (!png) {
        return false;
    }
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    bool ok = false;
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        ok = WriteFile(file, png, (DWORD)pngSize, &written, NULL) && written == pngSize;
        CloseHandle(file);
        if (!ok) {
            DeleteFileW(path);
        }
    }
    free(png);
    return ok;
}

bool clipboard_copy_region(HWND owner, const Canvas *canvas, RECT region, bool opaqueBackdrop)
{
    int width;
    int height;
    uint8_t *rgba = region_to_rgba(canvas, region, opaqueBackdrop, &width, &height);
    if (!rgba) {
        return false;
    }
    size_t pngSize = 0;
    uint8_t *png = png_encode_rgba(rgba, width, height, &pngSize);

    size_t dibSize = sizeof(BITMAPINFOHEADER) + (size_t)width * height * 4;
    uint8_t *dib = malloc(dibSize);
    if (dib) {
        BITMAPINFOHEADER header = canvas_dib_header(width, height);
        memcpy(dib, &header, sizeof header);
        uint8_t *pixels = dib + sizeof header;
        for (int y = 0; y < height; y++) {
            const uint8_t *source = rgba + (size_t)(height - 1 - y) * width * 4;
            uint8_t *target = pixels + (size_t)y * width * 4;
            for (int x = 0; x < width; x++) {
                target[x * 4] = source[x * 4 + 2];
                target[x * 4 + 1] = source[x * 4 + 1];
                target[x * 4 + 2] = source[x * 4];
                target[x * 4 + 3] = source[x * 4 + 3];
            }
        }
    }
    free(rgba);

    HGLOBAL pngHandle = png ? global_copy(png, pngSize) : NULL;
    HGLOBAL dibHandle = dib ? global_copy(dib, dibSize) : NULL;
    free(png);
    free(dib);
    if (!pngHandle && !dibHandle) {
        return false;
    }
    bool ok = false;
    if (OpenClipboard(owner)) {
        if (EmptyClipboard()) {
            ok = clipboard_put(RegisterClipboardFormatW(L"PNG"), pngHandle);
            ok = clipboard_put(CF_DIB, dibHandle) || ok;
            pngHandle = NULL;
            dibHandle = NULL;
        }
        CloseClipboard();
    }
    if (pngHandle) {
        GlobalFree(pngHandle);
    }
    if (dibHandle) {
        GlobalFree(dibHandle);
    }
    return ok;
}
