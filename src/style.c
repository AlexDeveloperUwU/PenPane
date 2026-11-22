/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "style.h"

#include <math.h>

const Color STROKE_PALETTE[STROKE_PALETTE_SIZE] = {
    COLOR_STROKE_DEFAULT, COLOR_STROKE_RED, COLOR_STROKE_GREEN, COLOR_STROKE_BLUE,
    COLOR_STROKE_ORANGE,
};

const Color FILL_PALETTE[FILL_PALETTE_SIZE] = {
    0, COLOR_FILL_RED, COLOR_FILL_GREEN, COLOR_FILL_BLUE, COLOR_FILL_YELLOW,
};

const float WIDTH_PRESETS[WIDTH_PRESET_COUNT] = {STROKE_WIDTH_THIN, STROKE_WIDTH_MEDIUM,
                                                 STROKE_WIDTH_BOLD};

Style style_default(void)
{
    Style style = {STROKE_PALETTE[0], 0, WIDTH_PRESETS[1], 1.0f, STROKE_SOLID, true, TEXT_FACE_HANDWRITTEN};
    return style;
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

int style_width_index(float width)
{
    int best = 0;
    for (int i = 1; i < WIDTH_PRESET_COUNT; i++) {
        if (fabsf(WIDTH_PRESETS[i] - width) < fabsf(WIDTH_PRESETS[best] - width)) {
            best = i;
        }
    }
    return best;
}
