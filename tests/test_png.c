/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/png.h"

/**
 * @brief Reads a big-endian 32-bit value.
 * @param bytes Four bytes.
 * @return Decoded value.
 */
static uint32_t read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
}

/**
 * @brief Computes a CRC-32 independently of the encoder.
 * @param data Bytes to checksum.
 * @param length Number of bytes.
 * @return Standard CRC-32 as used by PNG.
 */
static uint32_t reference_crc(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/**
 * @brief Encodes an image, walks every chunk, verifies its CRC and recovers the pixel rows.
 * @param width Image width.
 * @param height Image height.
 */
static void round_trip(int width, int height)
{
    size_t rowBytes = (size_t)width * 4;
    uint8_t *rgba = malloc(rowBytes * (size_t)height);
    assert(rgba);
    for (size_t i = 0; i < rowBytes * (size_t)height; i++) {
        rgba[i] = (uint8_t)(i * 7 + i / 251);
    }
    size_t size = 0;
    uint8_t *png = png_encode_rgba(rgba, width, height, &size);
    assert(png);
    static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    assert(size > 8 && memcmp(png, SIGNATURE, 8) == 0);

    uint8_t *inflated = malloc(rowBytes * (size_t)height + (size_t)height + 16);
    assert(inflated);
    size_t inflatedSize = 0;
    bool sawHeader = false;
    bool sawEnd = false;
    size_t at = 8;
    while (at < size) {
        uint32_t length = read_u32(png + at);
        const uint8_t *type = png + at + 4;
        const uint8_t *data = png + at + 8;
        assert(at + 12 + length <= size);
        assert(read_u32(data + length) == reference_crc(type, length + 4));
        if (memcmp(type, "IHDR", 4) == 0) {
            assert(length == 13 && read_u32(data) == (uint32_t)width &&
                   read_u32(data + 4) == (uint32_t)height);
            assert(data[8] == 8 && data[9] == 6);
            sawHeader = true;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            assert(data[0] == 0x78);
            size_t position = 2;
            bool last = false;
            while (!last) {
                last = (data[position] & 1) != 0;
                assert((data[position] & 6) == 0);
                uint32_t block = data[position + 1] | ((uint32_t)data[position + 2] << 8);
                assert((uint16_t)~block == (uint16_t)(data[position + 3] | (data[position + 4] << 8)));
                memcpy(inflated + inflatedSize, data + position + 5, block);
                inflatedSize += block;
                position += 5 + block;
            }
            assert(position + 4 == length);
        } else if (memcmp(type, "IEND", 4) == 0) {
            assert(length == 0);
            sawEnd = true;
        }
        at += 12 + length;
    }
    assert(sawHeader && sawEnd && at == size);
    assert(inflatedSize == (rowBytes + 1) * (size_t)height);
    for (int y = 0; y < height; y++) {
        assert(inflated[(size_t)y * (rowBytes + 1)] == 0);
        assert(memcmp(inflated + (size_t)y * (rowBytes + 1) + 1, rgba + (size_t)y * rowBytes, rowBytes) == 0);
    }
    free(inflated);
    free(png);
    free(rgba);
}

int main(void)
{
    round_trip(1, 1);
    round_trip(3, 2);
    round_trip(200, 100);
    round_trip(17, 1200);
    printf("png tests passed\n");
    return 0;
}
