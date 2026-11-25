/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>

#include "../src/canvas.h"
#include "../src/element.h"
#include "../src/render.h"

#define CANVAS_SIZE 200

/**
 * @brief Renders a white rectangle with the given corners and reports whether its outer corner pixel is
 * inked.
 * @param corners Corner shape of the rectangle.
 * @return true when the pixel at the bounding box corner has coverage.
 */
static bool corner_inked(Corners corners)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_SIZE, CANVAS_SIZE));
    Style style = style_default();
    style.corners = corners;
    Element element = element_make(ELEMENT_RECTANGLE, &style);
    element.from = (Point){20.0f, 20.0f};
    element.to = (Point){180.0f, 180.0f};
    RECT clip = canvas_area(&canvas);
    render_element(&canvas, &element, &clip);
    bool inked = (canvas.pixels[20 * CANVAS_SIZE + 20] >> 24) != 0;
    canvas_destroy(&canvas);
    return inked;
}

/**
 * @brief Sharp rectangles ink the corner of their box; rounded ones leave it empty.
 */
static void test_rectangle_corners(void)
{
    assert(!corner_inked(CORNERS_ROUND));
    assert(corner_inked(CORNERS_SHARP));
}

int main(void)
{
    test_rectangle_corners();
    printf("render tests passed\n");
    return 0;
}
