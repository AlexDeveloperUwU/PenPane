/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file clipboard.h
 * @brief Copies a rendered drawing to the clipboard as PNG (and as a bitmap for older applications).
 */
#ifndef PENPANE_CLIPBOARD_H
#define PENPANE_CLIPBOARD_H

#include <stdbool.h>
#include <windows.h>

#include "canvas.h"

/**
 * @brief Places a region of a canvas on the clipboard.
 * @param owner Window that owns the clipboard while it is open.
 * @param canvas Source canvas holding premultiplied pixels.
 * @param region Region to copy, clipped to the canvas.
 * @param opaqueBackdrop true when the region is fully opaque (whiteboard); false keeps transparency in the PNG.
 * @return true on success.
 */
bool clipboard_copy_region(HWND owner, const Canvas *canvas, RECT region, bool opaqueBackdrop);

/**
 * @brief Writes a region of a canvas to a PNG file, replacing an existing file.
 * @param canvas Source canvas holding premultiplied pixels.
 * @param region Region to save, clipped to the canvas.
 * @param opaqueBackdrop true when the region is fully opaque (whiteboard); false keeps transparency.
 * @param path Destination file path.
 * @return true when the whole file was written; a partial file is deleted.
 */
bool clipboard_save_region(const Canvas *canvas, RECT region, bool opaqueBackdrop, const wchar_t *path);

#endif
