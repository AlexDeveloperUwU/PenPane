/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>

#include "../src/style.h"
#include "../src/text.h"

/**
 * @brief Offsets follow the alignment and never go negative.
 */
static void test_line_offset(void)
{
    assert(text_line_offset(100, 40, TEXT_ALIGN_LEFT) == 0);
    assert(text_line_offset(100, 40, TEXT_ALIGN_CENTER) == 30);
    assert(text_line_offset(100, 40, TEXT_ALIGN_RIGHT) == 60);
    assert(text_line_offset(100, 100, TEXT_ALIGN_RIGHT) == 0);
    assert(text_line_offset(40, 100, TEXT_ALIGN_CENTER) == 0);
    assert(text_line_offset(100, 40, 99) == 0);
}

/**
 * @brief Finds the leftmost inked column of the second line of a two line mask.
 * @param align Alignment passed to the rasterizer.
 * @param blockWidth Receives the mask width.
 * @return Column of the first inked pixel on the second line, or -1 when it has none.
 */
static int second_line_start(int align, int *blockWidth)
{
    Mask mask;
    assert(text_make_mask_aligned(&mask, TEXT_FONT_UI, L"WWWWWWWW\nW", 20, 0, 0, align));
    *blockWidth = mask.width;
    int start = -1;
    for (int row = mask.height / 2 + 1; row < mask.height && start < 0; row++) {
        for (int column = 0; column < mask.width; column++) {
            if (mask.coverage[row * mask.width + column] > 64) {
                start = column;
                break;
            }
        }
    }
    mask_destroy(&mask);
    return start;
}

/**
 * @brief The short second line sits at the left, middle or right of the block.
 */
static void test_mask_alignment(void)
{
    int width;
    int left = second_line_start(TEXT_ALIGN_LEFT, &width);
    int center = second_line_start(TEXT_ALIGN_CENTER, &width);
    int right = second_line_start(TEXT_ALIGN_RIGHT, &width);
    assert(left >= 0 && left < width / 4);
    assert(center > left + width / 4 && center < right);
    assert(right > width / 2);
}

int main(void)
{
    test_line_offset();
    test_mask_alignment();
    printf("text tests passed\n");
    return 0;
}
