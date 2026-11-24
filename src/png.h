/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file png.h
 * @brief Minimal PNG encoder: uncompressed (stored) deflate, no dependencies.
 */
#ifndef PENPANE_PNG_H
#define PENPANE_PNG_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encodes straight-alpha RGBA rows as an uncompressed PNG.
 * @param rgba Pixel rows, width * 4 bytes each.
 * @param width Image width.
 * @param height Image height.
 * @param size Receives the PNG size in bytes.
 * @return Newly allocated PNG data to release with free, or NULL.
 */
uint8_t *png_encode_rgba(const uint8_t *rgba, int width, int height, size_t *size);

#endif
