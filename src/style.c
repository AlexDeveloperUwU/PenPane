/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "style.h"

#include <math.h>
#include <stdlib.h>

const Color STROKE_PALETTE[STROKE_PALETTE_SIZE] = {
    COLOR_STROKE_DEFAULT, COLOR_STROKE_RED, COLOR_STROKE_GREEN, COLOR_STROKE_BLUE, COLOR_STROKE_YELLOW,
};

const Color PASTEL_PALETTE[PASTEL_PALETTE_SIZE] = {
    0xFFFF9B9Bu, 0xFFFFBE8Au, 0xFFFFE58Fu, 0xFF9FDDA8u, 0xFF9CCBF7u, 0xFFC9B3F2u,
};

const Color FILL_PALETTE[FILL_PALETTE_SIZE] = {
    0, COLOR_FILL_RED, COLOR_FILL_GREEN, COLOR_FILL_BLUE, COLOR_FILL_YELLOW,
};

const float WIDTH_PRESETS[WIDTH_PRESET_COUNT] = {STROKE_WIDTH_THIN, STROKE_WIDTH_MEDIUM, STROKE_WIDTH_BOLD};

const float HIGHLIGHTER_WIDTHS[WIDTH_PRESET_COUNT] = {HIGHLIGHTER_WIDTH_THIN, HIGHLIGHTER_WIDTH_MEDIUM,
                                                      HIGHLIGHTER_WIDTH_BOLD};

const int FONT_SIZES[FONT_SIZE_COUNT] = {16, 20, 28, 36};

Style style_default(void)
{
    Style style = {
        STROKE_PALETTE[0], 0,     WIDTH_PRESETS[1], 1.0f, STROKE_SOLID, true, TEXT_FACE_HANDWRITTEN,
        ARROW_HEAD_END,    false, FONT_SIZES[1]};
    return style;
}

int style_font_size_index(const Style *style)
{
    int best = 0;
    for (int i = 1; i < FONT_SIZE_COUNT; i++) {
        if (abs(FONT_SIZES[i] - style->fontSize) < abs(FONT_SIZES[best] - style->fontSize)) {
            best = i;
        }
    }
    return best;
}

float style_width_preset(const Style *style, int index)
{
    return style->highlighter ? HIGHLIGHTER_WIDTHS[index] : WIDTH_PRESETS[index];
}

int style_stroke_index(Color color)
{
    for (int i = 0; i < STROKE_PALETTE_SIZE; i++) {
        if (STROKE_PALETTE[i] == color) {
            return i;
        }
    }
    return -1;
}

int style_fill_index(Color color)
{
    for (int i = 0; i < FILL_PALETTE_SIZE; i++) {
        if (FILL_PALETTE[i] == color) {
            return i;
        }
    }
    return -1;
}

Color color_from_hsv(float hue, float saturation, float value)
{
    hue = isfinite(hue) ? hue - floorf(hue) : 0.0f;
    saturation = fminf(fmaxf(saturation, 0.0f), 1.0f);
    value = fminf(fmaxf(value, 0.0f), 1.0f);
    float scaled = hue * 6.0f;
    int sector = (int)scaled;
    float fraction = scaled - (float)sector;
    float p = value * (1.0f - saturation);
    float q = value * (1.0f - saturation * fraction);
    float t = value * (1.0f - saturation * (1.0f - fraction));
    float red = value;
    float green = t;
    float blue = p;
    switch (sector % 6) {
    case 1:
        red = q;
        green = value;
        blue = p;
        break;
    case 2:
        red = p;
        green = value;
        blue = t;
        break;
    case 3:
        red = p;
        green = q;
        blue = value;
        break;
    case 4:
        red = t;
        green = p;
        blue = value;
        break;
    case 5:
        red = value;
        green = p;
        blue = q;
        break;
    default:
        break;
    }
    return 0xFF000000u | ((Color)lroundf(red * 255.0f) << 16) | ((Color)lroundf(green * 255.0f) << 8) |
           (Color)lroundf(blue * 255.0f);
}

void color_to_hsv(Color color, float *hue, float *saturation, float *value)
{
    float red = (float)((color >> 16) & 0xFFu) / 255.0f;
    float green = (float)((color >> 8) & 0xFFu) / 255.0f;
    float blue = (float)(color & 0xFFu) / 255.0f;
    float high = fmaxf(red, fmaxf(green, blue));
    float delta = high - fminf(red, fminf(green, blue));
    float h = 0.0f;
    if (delta > 0.0f) {
        if (high == red) {
            h = fmodf((green - blue) / delta, 6.0f);
        } else if (high == green) {
            h = (blue - red) / delta + 2.0f;
        } else {
            h = (red - green) / delta + 4.0f;
        }
        h /= 6.0f;
        h -= floorf(h);
    }
    *hue = h;
    *saturation = high > 0.0f ? delta / high : 0.0f;
    *value = high;
}

bool color_parse_hex(const char *text, Color *color)
{
    if (*text == '#') {
        text++;
    }
    Color result = 0;
    for (int i = 0; i < 6; i++, text++) {
        char c = *text;
        Color digit;
        if (c >= '0' && c <= '9') {
            digit = (Color)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (Color)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = (Color)(c - 'A' + 10);
        } else {
            return false;
        }
        result = result * 16u + digit;
    }
    if (*text != '\0') {
        return false;
    }
    *color = 0xFF000000u | result;
    return true;
}

int style_width_index(const Style *style)
{
    int best = 0;
    for (int i = 1; i < WIDTH_PRESET_COUNT; i++) {
        if (fabsf(style_width_preset(style, i) - style->width) <
            fabsf(style_width_preset(style, best) - style->width)) {
            best = i;
        }
    }
    return best;
}
