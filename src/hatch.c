/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hatch.h"

#include <math.h>

#define HATCH_GAP_PER_WIDTH 4.0f
#define HATCH_MIN_GAP 8.0f
#define HATCH_MIN_THICKNESS 1.5f
#define HATCH_THICKNESS_PER_WIDTH 1.0f
#define INV_SQRT2 0.70710678f

/**
 * @brief Coverage of one family of parallel lines at a point.
 * @param distance Signed position across the lines, in pixels.
 * @param gap Distance between neighbouring lines.
 * @param half Half of the line thickness.
 * @return Coverage from 0 to 1, antialiased over one pixel.
 */
static float line_coverage(float distance, float gap, float half)
{
    float offset = distance - gap * floorf(distance / gap);
    float nearest = offset < gap - offset ? offset : gap - offset;
    float coverage = half + 0.5f - nearest;
    return coverage < 0.0f ? 0.0f : (coverage > 1.0f ? 1.0f : coverage);
}

void mask_apply_hatch(Mask *mask, Point origin, FillStyle fillStyle, float strokeWidth)
{
    if (fillStyle == FILL_STYLE_SOLID) {
        return;
    }
    float gap = strokeWidth * HATCH_GAP_PER_WIDTH;
    gap = gap < HATCH_MIN_GAP ? HATCH_MIN_GAP : gap;
    float thickness = strokeWidth * HATCH_THICKNESS_PER_WIDTH;
    float half = (thickness < HATCH_MIN_THICKNESS ? HATCH_MIN_THICKNESS : thickness) * 0.5f;
    for (int row = 0; row < mask->height; row++) {
        float y = (float)(mask->y + row) + 0.5f - origin.y;
        uint8_t *line = mask->coverage + (size_t)row * (size_t)mask->width;
        for (int column = 0; column < mask->width; column++) {
            if (line[column] == 0) {
                continue;
            }
            float x = (float)(mask->x + column) + 0.5f - origin.x;
            float stripe = line_coverage((x + y) * INV_SQRT2, gap, half);
            if (fillStyle == FILL_STYLE_CROSS_HATCH) {
                float across = line_coverage((x - y) * INV_SQRT2, gap, half);
                stripe = across > stripe ? across : stripe;
            }
            line[column] = (uint8_t)lroundf((float)line[column] * stripe);
        }
    }
}
