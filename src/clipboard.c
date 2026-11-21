#include "clipboard.h"

#include <stdint.h>
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

/**
 * @brief Encodes straight-alpha RGBA rows as an uncompressed PNG.
 * @param rgba Pixel rows, width * 4 bytes each.
 * @param width Image width.
 * @param height Image height.
 * @param size Receives the PNG size in bytes.
 * @return Newly allocated PNG data or NULL.
 */
static uint8_t *encode_png(const uint8_t *rgba, int width, int height, size_t *size)
{
    size_t rowBytes = (size_t)width * 4 + 1;
    size_t rawSize = rowBytes * (size_t)height;
    size_t blocks = (rawSize + STORED_BLOCK_MAX - 1) / STORED_BLOCK_MAX;
    size_t zlibSize = 2 + rawSize + blocks * 5 + 4;
    uint8_t *raw = malloc(rawSize);
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

bool clipboard_copy_region(HWND owner, const Canvas *canvas, RECT region, bool opaqueBackdrop)
{
    if (region.left < 0) region.left = 0;
    if (region.top < 0) region.top = 0;
    if (region.right > canvas->width) region.right = canvas->width;
    if (region.bottom > canvas->height) region.bottom = canvas->height;
    int width = region.right - region.left;
    int height = region.bottom - region.top;
    if (width <= 0 || height <= 0) {
        return false;
    }
    uint8_t *rgba = malloc((size_t)width * (size_t)height * 4);
    if (!rgba) {
        return false;
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
    size_t pngSize = 0;
    uint8_t *png = encode_png(rgba, width, height, &pngSize);

    size_t dibSize = sizeof(BITMAPINFOHEADER) + (size_t)width * height * 4;
    uint8_t *dib = malloc(dibSize);
    if (dib) {
        BITMAPINFOHEADER header = {0};
        header.biSize = sizeof header;
        header.biWidth = width;
        header.biHeight = height;
        header.biPlanes = 1;
        header.biBitCount = 32;
        header.biCompression = BI_RGB;
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
