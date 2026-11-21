/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "text.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define FONT_CACHE_SIZE 16
#define SURFACE_TRIM_BYTES (4 * 1024 * 1024)
#define SURFACE_ROUNDING 64
#define MASK_PADDING 2
#define MASK_MAX_PIXELS (32 * 1024 * 1024)

/** @brief One cached GDI font. */
typedef struct CachedFont {
    TextFont role;
    int size;
    HFONT font;
} CachedFont;

/** @brief Reused scratch surface that GDI draws white-on-black text into. */
typedef struct Surface {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    uint32_t *bits;
    int width;
    int height;
} Surface;

static CachedFont g_fonts[FONT_CACHE_SIZE];
static int g_fontCount;
static int g_fontNext;
static Surface g_surface;
static int g_canvasFaceKnown;
static const wchar_t *g_canvasFace = L"Segoe UI";

/**
 * @brief Records that a font family exists; used as the enumeration callback.
 * @param logicalFont Enumerated font (unused).
 * @param metric Text metrics (unused).
 * @param type Font type (unused).
 * @param parameter Pointer to the int flag that is set.
 * @return 0 to stop enumeration.
 */
static int CALLBACK font_found(const LOGFONTW *logicalFont, const TEXTMETRICW *metric, DWORD type,
                               LPARAM parameter)
{
    (void)logicalFont;
    (void)metric;
    (void)type;
    *(int *)parameter = 1;
    return 0;
}

/**
 * @brief Tells whether a font family is installed.
 * @param dc Device context used for enumeration.
 * @param face Family name.
 * @return true when the family exists.
 */
static bool font_exists(HDC dc, const wchar_t *face)
{
    LOGFONTW query = {0};
    int found = 0;
    query.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(query.lfFaceName, face, LF_FACESIZE - 1);
    EnumFontFamiliesExW(dc, &query, font_found, (LPARAM)&found, 0);
    return found != 0;
}

/**
 * @brief Creates the scratch device context on first use.
 * @return true when a device context is available.
 */
static bool surface_ensure_dc(void)
{
    if (!g_surface.dc) {
        g_surface.dc = CreateCompatibleDC(NULL);
        if (g_surface.dc) {
            SetBkMode(g_surface.dc, TRANSPARENT);
            SetTextColor(g_surface.dc, RGB(255, 255, 255));
        }
    }
    return g_surface.dc != NULL;
}

/**
 * @brief Chooses the canvas font family once.
 */
static void resolve_canvas_face(void)
{
    if (g_canvasFaceKnown || !surface_ensure_dc()) {
        return;
    }
    g_canvasFaceKnown = 1;
    if (font_exists(g_surface.dc, L"Segoe Print")) {
        g_canvasFace = L"Segoe Print";
    }
}

/**
 * @brief Looks up or creates the GDI font for a role and size.
 * @param role Font role.
 * @param size Em height in pixels.
 * @return Font handle owned by the cache, or NULL on failure.
 */
static HFONT font_get(TextFont role, int size)
{
    for (int i = 0; i < g_fontCount; i++) {
        if (g_fonts[i].role == role && g_fonts[i].size == size) {
            return g_fonts[i].font;
        }
    }
    resolve_canvas_face();
    const wchar_t *face = role == TEXT_FONT_CANVAS ? g_canvasFace : L"Segoe UI";
    HFONT font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, face);
    if (!font) {
        return NULL;
    }
    int slot = g_fontCount;
    if (g_fontCount < FONT_CACHE_SIZE) {
        g_fontCount++;
    } else {
        slot = g_fontNext;
        g_fontNext = (g_fontNext + 1) % FONT_CACHE_SIZE;
        DeleteObject(g_fonts[slot].font);
    }
    g_fonts[slot].role = role;
    g_fonts[slot].size = size;
    g_fonts[slot].font = font;
    return font;
}

/**
 * @brief Releases the scratch bitmap but keeps the device context.
 */
static void surface_release_bitmap(void)
{
    if (g_surface.bitmap) {
        SelectObject(g_surface.dc, g_surface.previous);
        DeleteObject(g_surface.bitmap);
    }
    g_surface.bitmap = NULL;
    g_surface.bits = NULL;
    g_surface.width = 0;
    g_surface.height = 0;
}

/**
 * @brief Makes sure the scratch surface can hold the given size, growing it when needed.
 * @param width Required width in pixels.
 * @param height Required height in pixels.
 * @return true when the surface is large enough.
 */
static bool surface_ensure(int width, int height)
{
    if (!surface_ensure_dc()) {
        return false;
    }
    if (g_surface.bitmap && g_surface.width >= width && g_surface.height >= height) {
        return true;
    }
    int newWidth = (width + SURFACE_ROUNDING - 1) / SURFACE_ROUNDING * SURFACE_ROUNDING;
    int newHeight = (height + SURFACE_ROUNDING - 1) / SURFACE_ROUNDING * SURFACE_ROUNDING;
    if (g_surface.width > newWidth) {
        newWidth = g_surface.width;
    }
    if (g_surface.height > newHeight) {
        newHeight = g_surface.height;
    }
    surface_release_bitmap();

    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = newWidth;
    info.bmiHeader.biHeight = -newHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    g_surface.bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_surface.bitmap) {
        return false;
    }
    g_surface.previous = SelectObject(g_surface.dc, g_surface.bitmap);
    g_surface.bits = bits;
    g_surface.width = newWidth;
    g_surface.height = newHeight;
    return true;
}

/**
 * @brief Finds the end of the line that starts at the given position.
 * @param text Start of the line.
 * @param length Receives the line length without the line break characters.
 * @return Pointer to the start of the next line, or NULL after the last line.
 */
static const wchar_t *line_next(const wchar_t *text, int *length)
{
    int count = 0;
    while (text[count] && text[count] != L'\n') {
        count++;
    }
    *length = (count > 0 && text[count - 1] == L'\r') ? count - 1 : count;
    return text[count] ? text + count + 1 : NULL;
}

/**
 * @brief Measures text with the font already selected into the scratch device context.
 * @param text Text to measure.
 * @param width Receives the widest line.
 * @param lineHeight Receives the height of one line.
 * @param lines Receives the number of lines.
 */
static void measure_selected(const wchar_t *text, int *width, int *lineHeight, int *lines)
{
    TEXTMETRICW metrics;
    GetTextMetricsW(g_surface.dc, &metrics);
    *lineHeight = metrics.tmHeight;
    *width = 0;
    *lines = 0;
    const wchar_t *cursor = text;
    while (cursor) {
        int length;
        const wchar_t *next = line_next(cursor, &length);
        SIZE extent = {0, 0};
        if (length > 0) {
            GetTextExtentPoint32W(g_surface.dc, cursor, length, &extent);
        }
        if (extent.cx > *width) {
            *width = extent.cx;
        }
        (*lines)++;
        cursor = next;
    }
}

void text_measure(TextFont font, const wchar_t *text, int pixelSize, int *width, int *height)
{
    *width = 0;
    *height = 0;
    HFONT handle = font_get(font, pixelSize);
    if (!handle || !text || !surface_ensure_dc()) {
        return;
    }
    HGDIOBJ previous = SelectObject(g_surface.dc, handle);
    int lineHeight;
    int lines;
    measure_selected(text, width, &lineHeight, &lines);
    *height = lineHeight * lines;
    SelectObject(g_surface.dc, previous);
}

bool text_make_mask(Mask *mask, TextFont font, const wchar_t *text, int pixelSize, int x, int y)
{
    memset(mask, 0, sizeof *mask);
    HFONT handle = font_get(font, pixelSize);
    if (!handle || !text || !text[0] || !surface_ensure_dc()) {
        return false;
    }
    HGDIOBJ previous = SelectObject(g_surface.dc, handle);
    int textWidth;
    int lineHeight;
    int lines;
    measure_selected(text, &textWidth, &lineHeight, &lines);
    int width = textWidth + MASK_PADDING;
    int height = lineHeight * lines;
    bool ok = width > MASK_PADDING && height > 0 && (long long)width * height <= MASK_MAX_PIXELS &&
              surface_ensure(width, height);
    if (ok) {
        for (int row = 0; row < height; row++) {
            memset(g_surface.bits + (size_t)row * g_surface.width, 0, (size_t)width * 4);
        }
        int top = 0;
        const wchar_t *cursor = text;
        while (cursor) {
            int length;
            const wchar_t *next = line_next(cursor, &length);
            if (length > 0) {
                TextOutW(g_surface.dc, 0, top, cursor, length);
            }
            top += lineHeight;
            cursor = next;
        }
        GdiFlush();
        ok = mask_create(mask, x, y, width, height);
    }
    SelectObject(g_surface.dc, previous);
    if (ok) {
        for (int row = 0; row < height; row++) {
            const uint32_t *source = g_surface.bits + (size_t)row * g_surface.width;
            uint8_t *target = mask->coverage + (size_t)row * width;
            for (int column = 0; column < width; column++) {
                target[column] = (uint8_t)((source[column] >> 8) & 0xFF);
            }
        }
    }
    if ((size_t)g_surface.width * g_surface.height * 4 > SURFACE_TRIM_BYTES) {
        surface_release_bitmap();
    }
    return ok;
}

void text_shutdown(void)
{
    surface_release_bitmap();
    if (g_surface.dc) {
        DeleteDC(g_surface.dc);
    }
    memset(&g_surface, 0, sizeof g_surface);
    for (int i = 0; i < g_fontCount; i++) {
        DeleteObject(g_fonts[i].font);
    }
    memset(g_fonts, 0, sizeof g_fonts);
    g_fontCount = 0;
    g_fontNext = 0;
    g_canvasFaceKnown = 0;
    g_canvasFace = L"Segoe UI";
}
