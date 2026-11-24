/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "png.h"

#include <stdlib.h>
#include <string.h>

#define STORED_BLOCK_MAX 65535u

/**
 * @brief Updates a CRC-32 checksum.
 * @param crc Running checksum, initially 0.
 * @param data Bytes to add.
 * @param length Number of bytes.
 * @return New checksum.
 */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    crc = ~crc;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return ~crc;
}

/**
 * @brief Appends big-endian 32-bit value.
 * @param out Destination buffer.
 * @param value Value to write.
 */
static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

/**
 * @brief Writes one PNG chunk.
 * @param out Destination buffer with room for length + 12 bytes.
 * @param type Four-character chunk type.
 * @param data Chunk payload.
 * @param length Payload length.
 * @return Number of bytes written.
 */
static size_t put_chunk(uint8_t *out, const char *type, const uint8_t *data, size_t length)
{
    put_u32(out, (uint32_t)length);
    memcpy(out + 4, type, 4);
    if (length) {
        memcpy(out + 8, data, length);
    }
    uint32_t crc = crc32_update(0, out + 4, length + 4);
    put_u32(out + 8 + length, crc);
    return length + 12;
}

uint8_t *png_encode_rgba(const uint8_t *rgba, int width, int height, size_t *size)
{
    size_t rowBytes = (size_t)width * 4 + 1;
    size_t rawSize = rowBytes * (size_t)height;
    size_t blocks = (rawSize + STORED_BLOCK_MAX - 1) / STORED_BLOCK_MAX;
    size_t zlibSize = 2 + rawSize + blocks * 5 + 4;
    uint8_t *raw = calloc(rawSize ? rawSize : 1, 1);
    uint8_t *png = malloc(8 + 25 + zlibSize + 12 + 12);
    uint8_t *zlib = malloc(zlibSize);
    if (!raw || !png || !zlib) {
        free(raw);
        free(png);
        free(zlib);
        return NULL;
    }
    for (int y = 0; y < height; y++) {
        raw[(size_t)y * rowBytes] = 0;
        memcpy(raw + (size_t)y * rowBytes + 1, rgba + (size_t)y * width * 4, (size_t)width * 4);
    }
    size_t position = 0;
    zlib[position++] = 0x78;
    zlib[position++] = 0x01;
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < rawSize; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    for (size_t offset = 0; offset < rawSize; offset += STORED_BLOCK_MAX) {
        size_t chunk = rawSize - offset < STORED_BLOCK_MAX ? rawSize - offset : STORED_BLOCK_MAX;
        zlib[position++] = offset + chunk >= rawSize ? 1 : 0;
        zlib[position++] = (uint8_t)chunk;
        zlib[position++] = (uint8_t)(chunk >> 8);
        zlib[position++] = (uint8_t)~chunk;
        zlib[position++] = (uint8_t)(~chunk >> 8);
        memcpy(zlib + position, raw + offset, chunk);
        position += chunk;
    }
    put_u32(zlib + position, (b << 16) | a);
    position += 4;

    static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    size_t total = 0;
    memcpy(png, SIGNATURE, 8);
    total += 8;
    uint8_t header[13];
    put_u32(header, (uint32_t)width);
    put_u32(header + 4, (uint32_t)height);
    header[8] = 8;
    header[9] = 6;
    header[10] = header[11] = header[12] = 0;
    total += put_chunk(png + total, "IHDR", header, 13);
    total += put_chunk(png + total, "IDAT", zlib, position);
    total += put_chunk(png + total, "IEND", NULL, 0);
    free(raw);
    free(zlib);
    *size = total;
    return png;
}
