/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file render_preview.c
 * @brief Headless rendering harness: draws reference scenes to PNG files and prints measurements.
 *
 * Usage: render_preview OUTPUT_DIRECTORY
 *
 * Writes shapes.png, pens.png, sample.png, trails.png and selection.png, and prints stroke thickness tables
 * and pixel dumps in the same format that was used to measure Excalidraw, so the two can be compared.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "element.h"
#include "pen.h"
#include "render.h"
#include "trail.h"

#define SCENE_WIDTH 1024
#define SCENE_HEIGHT 768
#define LUMA_BACKDROP 18.0f
#define LUMA_STROKE 211.0f

/**
 * @brief Computes the CRC-32 of a byte range as required by PNG chunks.
 * @param crc Running value, 0xFFFFFFFF for a new chunk.
 * @param data Bytes to add.
 * @param length Number of bytes.
 * @return Updated running value.
 */
static uint32_t crc_update(uint32_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

/**
 * @brief Writes a big-endian 32-bit value.
 * @param file Output file.
 * @param value Value to write.
 */
static void put32(FILE *file, uint32_t value)
{
    uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8),
                        (uint8_t)value};
    fwrite(bytes, 1, 4, file);
}

/**
 * @brief Writes one PNG chunk.
 * @param file Output file.
 * @param type Four character chunk type.
 * @param data Chunk payload.
 * @param length Payload size.
 */
static void put_chunk(FILE *file, const char *type, const uint8_t *data, size_t length)
{
    put32(file, (uint32_t)length);
    fwrite(type, 1, 4, file);
    if (length) {
        fwrite(data, 1, length, file);
    }
    uint32_t crc = crc_update(0xFFFFFFFFu, (const uint8_t *)type, 4);
    crc = crc_update(crc, data, length);
    put32(file, ~crc);
}

/**
 * @brief Saves a rectangle of a canvas as an uncompressed PNG, optionally magnified.
 * @param canvas Source canvas, treated as opaque.
 * @param path Output file path.
 * @param left Left of the rectangle.
 * @param top Top of the rectangle.
 * @param width Width of the rectangle.
 * @param height Height of the rectangle.
 * @param zoom Integer nearest-neighbour magnification.
 * @return true on success.
 */
static bool save_png(const Canvas *canvas, const char *path, int left, int top, int width, int height,
                     int zoom)
{
    int outWidth = width * zoom;
    int outHeight = height * zoom;
    size_t rowBytes = (size_t)outWidth * 3 + 1;
    size_t rawSize = rowBytes * (size_t)outHeight;
    uint8_t *raw = malloc(rawSize);
    if (!raw) {
        return false;
    }
    for (int y = 0; y < outHeight; y++) {
        uint8_t *row = raw + rowBytes * (size_t)y;
        row[0] = 0;
        for (int x = 0; x < outWidth; x++) {
            int sx = left + x / zoom;
            int sy = top + y / zoom;
            uint32_t pixel = canvas->pixels[(size_t)sy * canvas->width + sx];
            row[1 + x * 3] = (uint8_t)(pixel >> 16);
            row[2 + x * 3] = (uint8_t)(pixel >> 8);
            row[3 + x * 3] = (uint8_t)pixel;
        }
    }
    size_t blocks = (rawSize + 65534) / 65535;
    size_t zSize = 2 + rawSize + blocks * 5 + 4;
    uint8_t *z = malloc(zSize);
    if (!z) {
        free(raw);
        return false;
    }
    size_t at = 0;
    z[at++] = 0x78;
    z[at++] = 0x01;
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < rawSize; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    for (size_t offset = 0; offset < rawSize; offset += 65535) {
        size_t chunk = rawSize - offset < 65535 ? rawSize - offset : 65535;
        z[at++] = offset + chunk >= rawSize ? 1 : 0;
        z[at++] = (uint8_t)chunk;
        z[at++] = (uint8_t)(chunk >> 8);
        z[at++] = (uint8_t)~chunk;
        z[at++] = (uint8_t)(~chunk >> 8);
        memcpy(z + at, raw + offset, chunk);
        at += chunk;
    }
    uint32_t adler = (b << 16) | a;
    z[at++] = (uint8_t)(adler >> 24);
    z[at++] = (uint8_t)(adler >> 16);
    z[at++] = (uint8_t)(adler >> 8);
    z[at++] = (uint8_t)adler;
    FILE *file = fopen(path, "wb");
    if (!file) {
        free(raw);
        free(z);
        return false;
    }
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(signature, 1, 8, file);
    uint8_t header[13] = {(uint8_t)(outWidth >> 24),
                          (uint8_t)(outWidth >> 16),
                          (uint8_t)(outWidth >> 8),
                          (uint8_t)outWidth,
                          (uint8_t)(outHeight >> 24),
                          (uint8_t)(outHeight >> 16),
                          (uint8_t)(outHeight >> 8),
                          (uint8_t)outHeight,
                          8,
                          2,
                          0,
                          0,
                          0};
    put_chunk(file, "IHDR", header, 13);
    put_chunk(file, "IDAT", z, at);
    put_chunk(file, "IEND", NULL, 0);
    fclose(file);
    free(raw);
    free(z);
    return true;
}

/**
 * @brief Coverage of one pixel in the measurement convention used for Excalidraw.
 * @param canvas Rendered canvas.
 * @param x Pixel x.
 * @param y Pixel y.
 * @return Coverage in 0..1 derived from the red channel between backdrop and default stroke.
 */
static float coverage_at(const Canvas *canvas, int x, int y)
{
    uint32_t pixel = canvas->pixels[(size_t)y * canvas->width + x];
    return ((float)((pixel >> 16) & 0xFF) - LUMA_BACKDROP) / (LUMA_STROKE - LUMA_BACKDROP);
}

/**
 * @brief Sums coverage down a column, giving the stroke thickness there.
 * @param canvas Rendered canvas.
 * @param x Column.
 * @param top First row.
 * @param bottom Row after the last.
 * @return Thickness in pixels.
 */
static float column_thickness(const Canvas *canvas, int x, int top, int bottom)
{
    float sum = 0.0f;
    for (int y = top; y < bottom; y++) {
        sum += coverage_at(canvas, x, y);
    }
    return sum;
}

/**
 * @brief Prints a digit map of coverage in the format used for the Excalidraw dumps.
 * @param canvas Rendered canvas.
 * @param left Window left.
 * @param top Window top.
 * @param width Window width.
 * @param height Window height.
 */
static void dump_window(const Canvas *canvas, int left, int top, int width, int height)
{
    for (int y = top; y < top + height; y++) {
        for (int x = left; x < left + width; x++) {
            float c = coverage_at(canvas, x, y);
            if (c < 0.04f) {
                printf(" .");
            } else {
                int digit = (int)(c * 9.0f + 0.5f);
                printf(" %d", digit > 9 ? 9 : digit);
            }
        }
        printf("\n");
    }
}

/**
 * @brief Prints the run lengths of lit and unlit stretches along a band, as used for dash measurements.
 * @param canvas Rendered canvas.
 * @param left First column.
 * @param right Column after the last.
 * @param top First row of the band.
 * @param bottom Row after the last.
 */
static void print_runs(const Canvas *canvas, int left, int right, int top, int bottom)
{
    int current = -1;
    int start = left;
    for (int x = left; x < right; x++) {
        float best = 0.0f;
        for (int y = top; y < bottom; y++) {
            float c = coverage_at(canvas, x, y);
            best = c > best ? c : best;
        }
        int on = best > 0.5f;
        if (current < 0) {
            current = on;
            start = x;
        } else if (on != current) {
            printf("%c%d ", current ? '+' : '-', x - start);
            current = on;
            start = x;
        }
    }
    printf("%c%d\n", current ? '+' : '-', right - start);
}

/**
 * @brief Clears a canvas to the dark backdrop.
 * @param canvas Canvas to clear.
 */
static void clear_scene(Canvas *canvas)
{
    RECT all = {0, 0, canvas->width, canvas->height};
    canvas_fill(canvas, &all, COLOR_CANVAS_DARK);
}

/**
 * @brief Draws an element and frees it.
 * @param canvas Target canvas.
 * @param element Element to draw; released afterwards.
 */
static void draw_and_free(Canvas *canvas, Element *element)
{
    RECT all = {0, 0, canvas->width, canvas->height};
    render_element(canvas, element, &all);
    element_free(element);
}

/**
 * @brief Adds a shape element with endpoints and draws it.
 * @param canvas Target canvas.
 * @param type Element type.
 * @param style Element style.
 * @param x1 First x.
 * @param y1 First y.
 * @param x2 Second x.
 * @param y2 Second y.
 */
static void draw_shape(Canvas *canvas, ElementType type, const Style *style, float x1, float y1, float x2,
                       float y2)
{
    Element element = element_make(type, style);
    element.from = (Point){x1, y1};
    element.to = (Point){x2, y2};
    draw_and_free(canvas, &element);
}

/**
 * @brief Draws a horizontal pen stroke emulating pointer events at a fixed spacing.
 * @param canvas Target canvas.
 * @param width PenPane width preset.
 * @param spacing Distance between pointer events in pixels.
 * @param mode Pressure argument for the pen builder.
 * @param x Start x.
 * @param y Baseline y.
 * @param length Stroke length.
 */
static void draw_pen_line(Canvas *canvas, float width, float spacing, float mode, float x, float y,
                          float length)
{
    Style style = style_default();
    style.width = width;
    Element element = element_make(ELEMENT_PEN, &style);
    PenBuilder builder;
    pen_begin(&builder, &element, (Point){x, y}, mode);
    for (float offset = spacing; offset <= length; offset += spacing) {
        pen_add(&builder, &element, (Point){x + offset, y}, mode);
    }
    pen_finish(&builder, &element);
    draw_and_free(canvas, &element);
}

/**
 * @brief Renders the pen thickness table for variable and constant modes.
 * @param canvas Scratch canvas.
 */
static void run_pen_table(Canvas *canvas)
{
    static const float spacings[] = {1, 2, 3, 4, 6, 8, 12};
    static const float widths[] = {2, 4, 8};
    printf("Pen thickness, variable mode (PenPane width, spacing: start, 15%%, 35%%, 55%%, 95%%)\n");
    for (int w = 0; w < 3; w++) {
        for (int s = 0; s < 7; s++) {
            clear_scene(canvas);
            draw_pen_line(canvas, widths[w], spacings[s], PEN_PRESSURE_SIMULATED, 20, 40, 360);
            printf("W%.0f s%.0f:", widths[w], spacings[s]);
            static const float fractions[] = {0.03f, 0.15f, 0.35f, 0.55f, 0.95f};
            for (int f = 0; f < 5; f++) {
                printf(" %.2f", column_thickness(canvas, 20 + (int)(360 * fractions[f]), 0, 90));
            }
            printf("\n");
        }
    }
    printf("Pen thickness, constant mode\n");
    for (int w = 0; w < 3; w++) {
        clear_scene(canvas);
        draw_pen_line(canvas, widths[w], 5, PEN_PRESSURE_CONSTANT, 20, 40, 360);
        printf("W%.0f: %.2f %.2f\n", widths[w], column_thickness(canvas, 200, 0, 90),
               column_thickness(canvas, 21 + 3, 0, 90));
    }
}

/**
 * @brief Draws the shape reference scene, the same content that was measured in Excalidraw.
 * @param canvas Target canvas.
 */
static void draw_shapes_scene(Canvas *canvas)
{
    clear_scene(canvas);
    Style style = style_default();
    float widths[3] = {1, 2, 4};
    for (int k = 0; k < 3; k++) {
        style.width = widths[k];
        draw_shape(canvas, ELEMENT_RECTANGLE, &style, 20 + k * 240, 40, 220 + k * 240, 160);
    }
    style.width = 2;
    style.strokeStyle = STROKE_DASHED;
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 20, 200, 220, 320);
    style.strokeStyle = STROKE_DOTTED;
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 260, 200, 460, 320);
    style.strokeStyle = STROKE_SOLID;
    draw_shape(canvas, ELEMENT_ELLIPSE, &style, 500, 200, 700, 320);
    style.strokeStyle = STROKE_DASHED;
    draw_shape(canvas, ELEMENT_ELLIPSE, &style, 740, 200, 940, 320);
    style.strokeStyle = STROKE_SOLID;
    style.width = 1;
    draw_shape(canvas, ELEMENT_ARROW, &style, 20, 360, 220, 360);
    style.width = 2;
    draw_shape(canvas, ELEMENT_ARROW, &style, 260, 360, 460, 360);
    style.width = 4;
    draw_shape(canvas, ELEMENT_ARROW, &style, 500, 360, 700, 360);
    style.width = 2;
    draw_shape(canvas, ELEMENT_ARROW, &style, 740, 360, 860, 420);
    style.strokeStyle = STROKE_DASHED;
    draw_shape(canvas, ELEMENT_ARROW, &style, 20, 420, 220, 420);
    style.strokeStyle = STROKE_DOTTED;
    draw_shape(canvas, ELEMENT_ARROW, &style, 260, 420, 460, 420);
    style.strokeStyle = STROKE_SOLID;
    draw_shape(canvas, ELEMENT_ARROW, &style, 500, 420, 530, 420);
    draw_shape(canvas, ELEMENT_ARROW, &style, 560, 420, 620, 420);
    draw_shape(canvas, ELEMENT_ARROW, &style, 650, 420, 750, 420);
    draw_shape(canvas, ELEMENT_LINE, &style, 20, 480, 220, 480);
    style.strokeStyle = STROKE_DASHED;
    draw_shape(canvas, ELEMENT_LINE, &style, 260, 480, 460, 480);
    style.strokeStyle = STROKE_DOTTED;
    draw_shape(canvas, ELEMENT_LINE, &style, 500, 480, 700, 480);
    style.strokeStyle = STROKE_DASHED;
    style.width = 1;
    draw_shape(canvas, ELEMENT_LINE, &style, 740, 480, 940, 480);
    style.width = 4;
    draw_shape(canvas, ELEMENT_LINE, &style, 20, 520, 220, 520);
    style.strokeStyle = STROKE_DOTTED;
    draw_shape(canvas, ELEMENT_LINE, &style, 260, 520, 460, 520);
    style = style_default();
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 500, 520, 540, 560);
    style.stroke = COLOR_STROKE_RED;
    style.fill = COLOR_FILL_RED;
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 560, 520, 960, 720);
    style = style_default();
    style.stroke = COLOR_STROKE_BLUE;
    style.fill = COLOR_FILL_BLUE;
    draw_shape(canvas, ELEMENT_ELLIPSE, &style, 20, 580, 120, 680);
    style = style_default();
    style.stroke = COLOR_STROKE_GREEN;
    style.opacity = 0.5f;
    draw_shape(canvas, ELEMENT_ELLIPSE, &style, 140, 580, 240, 680);
}

/**
 * @brief Draws sample handwriting-like strokes at three widths and two speeds.
 * @param canvas Target canvas.
 */
static void draw_pens_scene(Canvas *canvas)
{
    clear_scene(canvas);
    static const float widths[3] = {1, 2, 4};
    for (int mode = 0; mode < 2; mode++) {
        for (int w = 0; w < 3; w++) {
            for (int speed = 0; speed < 2; speed++) {
                Style style = style_default();
                style.width = widths[w];
                Element element = element_make(ELEMENT_PEN, &style);
                PenBuilder builder;
                float pressure = mode == 0 ? PEN_PRESSURE_SIMULATED : PEN_PRESSURE_CONSTANT;
                float originX = 30 + speed * 480;
                float originY = 60 + (mode * 3 + w) * 110;
                float step = speed == 0 ? 2.0f : 9.0f;
                pen_begin(&builder, &element, (Point){originX, originY}, pressure);
                for (float t = step; t < 420; t += step) {
                    Point p = {originX + t, originY + 36.0f * sinf(t / 36.0f) * cosf(t / 120.0f)};
                    pen_add(&builder, &element, p, pressure);
                }
                pen_finish(&builder, &element);
                draw_and_free(canvas, &element);
            }
        }
    }
}

/**
 * @brief Draws a pen stroke along a parametric path.
 * @param canvas Target canvas.
 * @param stroke Stroke color.
 * @param width PenPane width preset.
 * @param centerX Center x of the path.
 * @param centerY Center y of the path.
 * @param radiusX Horizontal radius.
 * @param radiusY Vertical radius.
 * @param turns Number of turns around the center.
 * @param drift Horizontal drift per turn, in pixels.
 */
static void draw_loop_stroke(Canvas *canvas, Color stroke, float width, float centerX, float centerY,
                             float radiusX, float radiusY, float turns, float drift)
{
    Style style = style_default();
    style.stroke = stroke;
    style.width = width;
    Element element = element_make(ELEMENT_PEN, &style);
    PenBuilder builder;
    const float step = 0.06f;
    float end = turns * 6.2831853f;
    pen_begin(&builder, &element, (Point){centerX + radiusX, centerY}, PEN_PRESSURE_SIMULATED);
    for (float angle = step; angle <= end; angle += step) {
        Point p = {centerX + radiusX * cosf(angle) + drift * angle / 6.2831853f,
                   centerY + radiusY * sinf(angle)};
        pen_add(&builder, &element, p, PEN_PRESSURE_SIMULATED);
    }
    pen_finish(&builder, &element);
    draw_and_free(canvas, &element);
}

/**
 * @brief Draws a typical annotation: a highlighted region, an arrow, a circled item and a hand-drawn
 * underline.
 * @param canvas Target canvas.
 */
static void draw_sample_scene(Canvas *canvas)
{
    clear_scene(canvas);
    Style style = style_default();
    style.stroke = COLOR_STROKE_RED;
    style.width = 2;
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 90, 120, 480, 300);
    style.width = 4;
    draw_shape(canvas, ELEMENT_ARROW, &style, 760, 120, 500, 220);
    style = style_default();
    style.stroke = COLOR_STROKE_BLUE;
    style.fill = COLOR_FILL_BLUE;
    style.width = 2;
    draw_shape(canvas, ELEMENT_ELLIPSE, &style, 600, 340, 900, 520);
    style = style_default();
    style.stroke = COLOR_STROKE_GREEN;
    style.strokeStyle = STROKE_DASHED;
    style.width = 2;
    draw_shape(canvas, ELEMENT_LINE, &style, 90, 380, 480, 380);
    draw_loop_stroke(canvas, COLOR_STROKE_ORANGE, 2, 200, 470, 90, 22, 1.0f, 190);
    draw_loop_stroke(canvas, COLOR_STROKE_DEFAULT, 1, 300, 600, 120, 36, 3.0f, 0);
}

/**
 * @brief Draws a laser trail and an eraser trail mid-motion, plus a faded laser.
 * @param canvas Target canvas.
 */
static void draw_trails_scene(Canvas *canvas)
{
    clear_scene(canvas);
    Trail laser;
    Trail eraser;
    trail_init(&laser, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, COLOR_LASER, true);
    trail_init(&eraser, TRAIL_ERASER_RADIUS, TRAIL_ERASER_LIFETIME_MS, COLOR_ERASER_TRAIL, false);
    for (int i = 0; i < 60; i++) {
        float t = (float)i / 59.0f;
        Point p = {80 + 800 * t, 100 + 60 * sinf(t * 9.0f)};
        trail_add(&laser, p, (uint32_t)(i * 16));
    }
    for (int i = 0; i < 20; i++) {
        float t = (float)i / 19.0f;
        Point p = {80 + 300 * t, 300 + 40 * sinf(t * 6.0f)};
        trail_add(&eraser, p, (uint32_t)(i * 16));
    }
    Style style = style_default();
    style.stroke = COLOR_STROKE_RED;
    style.fill = COLOR_FILL_RED;
    draw_shape(canvas, ELEMENT_RECTANGLE, &style, 100, 260, 420, 360);
    RECT all = {0, 0, canvas->width, canvas->height};
    trail_render(&laser, canvas, &all, 60 * 16);
    trail_render(&eraser, canvas, &all, 20 * 16);
    Trail faded;
    trail_init(&faded, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, COLOR_LASER, true);
    for (int i = 0; i < 60; i++) {
        float t = (float)i / 59.0f;
        Point p = {80 + 800 * t, 500 + 60 * sinf(t * 9.0f)};
        trail_add(&faded, p, (uint32_t)(i * 16));
    }
    trail_render(&faded, canvas, &all, 60 * 16 + 550);
}

/**
 * @brief Draws selection decorations over a rectangle and an ellipse.
 * @param canvas Target canvas.
 */
static void draw_selection_scene(Canvas *canvas)
{
    clear_scene(canvas);
    Style style = style_default();
    Element ellipse = element_make(ELEMENT_ELLIPSE, &style);
    ellipse.from = (Point){100, 100};
    ellipse.to = (Point){300, 220};
    RECT all = {0, 0, canvas->width, canvas->height};
    render_element(canvas, &ellipse, &all);
    render_selection(canvas, &ellipse, &all);
    element_free(&ellipse);
}

/**
 * @brief Prints how long typical redraw workloads take.
 * @param canvas Scratch canvas.
 */
static void run_timing(Canvas *canvas)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER begin;
    LARGE_INTEGER end;
    QueryPerformanceFrequency(&frequency);
    clear_scene(canvas);
    Style style = style_default();
    QueryPerformanceCounter(&begin);
    for (int i = 0; i < 50; i++) {
        draw_shape(canvas, ELEMENT_ARROW, &style, 10, 10, 1000, 700);
    }
    QueryPerformanceCounter(&end);
    printf("50 diagonal arrows of 1200 px: %.1f ms\n",
           1000.0 * (double)(end.QuadPart - begin.QuadPart) / (double)frequency.QuadPart);
    QueryPerformanceCounter(&begin);
    for (int i = 0; i < 20; i++) {
        draw_pen_line(canvas, 4, 3, PEN_PRESSURE_SIMULATED, 20, 100 + i * 20, 900);
    }
    QueryPerformanceCounter(&end);
    printf("20 pen strokes of 300 points: %.1f ms\n",
           1000.0 * (double)(end.QuadPart - begin.QuadPart) / (double)frequency.QuadPart);
    QueryPerformanceCounter(&begin);
    for (int i = 0; i < 20; i++) {
        draw_shape(canvas, ELEMENT_RECTANGLE, &style, 20, 20, 1000, 700);
        style.fill = COLOR_FILL_RED;
    }
    QueryPerformanceCounter(&end);
    printf("20 filled rectangles of 980x680: %.1f ms\n",
           1000.0 * (double)(end.QuadPart - begin.QuadPart) / (double)frequency.QuadPart);
}
/**
 * @brief Entry point.
 * @param argc Argument count.
 * @param argv Arguments; argv[1] is the output directory.
 * @return 0 on success.
 */
int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : ".";
    Canvas canvas;
    if (!canvas_create(&canvas, SCENE_WIDTH, SCENE_HEIGHT)) {
        fprintf(stderr, "canvas creation failed\n");
        return 1;
    }
    char path[512];
    run_pen_table(&canvas);
    run_timing(&canvas);

    draw_shapes_scene(&canvas);
    snprintf(path, sizeof path, "%s\\shapes.png", directory);
    save_png(&canvas, path, 0, 0, SCENE_WIDTH, 740, 1);
    printf("corner (sw2 rect at 260,40):\n");
    dump_window(&canvas, 258, 38, 40, 36);
    printf("dash runs sw2 / dot sw2 / dash sw1 / dash sw4 / dot sw4:\n");
    print_runs(&canvas, 255, 470, 470, 490);
    print_runs(&canvas, 495, 710, 470, 490);
    print_runs(&canvas, 735, 950, 470, 490);
    print_runs(&canvas, 15, 230, 510, 530);
    print_runs(&canvas, 255, 470, 510, 530);
    printf("arrow sw2 head:\n");
    dump_window(&canvas, 428, 344, 36, 32);
    printf("arrow len30 head:\n");
    dump_window(&canvas, 496, 404, 40, 32);
    snprintf(path, sizeof path, "%s\\shapes_zoom.png", directory);
    save_png(&canvas, path, 240, 20, 260, 160, 3);
    snprintf(path, sizeof path, "%s\\shapes_zoom_arrows.png", directory);
    save_png(&canvas, path, 240, 340, 260, 100, 3);

    draw_pens_scene(&canvas);
    snprintf(path, sizeof path, "%s\\pens.png", directory);
    save_png(&canvas, path, 0, 0, SCENE_WIDTH, 740, 1);

    draw_sample_scene(&canvas);
    snprintf(path, sizeof path, "%s\\sample.png", directory);
    save_png(&canvas, path, 0, 0, SCENE_WIDTH, 740, 1);

    draw_trails_scene(&canvas);
    snprintf(path, sizeof path, "%s\\trails.png", directory);
    save_png(&canvas, path, 0, 0, SCENE_WIDTH, 600, 1);

    draw_selection_scene(&canvas);
    snprintf(path, sizeof path, "%s\\selection.png", directory);
    save_png(&canvas, path, 80, 80, 240, 160, 3);
    canvas_destroy(&canvas);
    return 0;
}
