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

/**
 * @brief Counts transparent and inked pixels along a row inside a filled, hatched rectangle.
 * @param fillStyle Fill style of the rectangle.
 * @param empty Receives the number of transparent pixels on the row.
 * @param inked Receives the number of inked pixels on the row.
 */
static void scan_fill_row(FillStyle fillStyle, int *empty, int *inked)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_SIZE, CANVAS_SIZE));
    Style style = style_default();
    style.fill = FILL_PALETTE[1];
    style.fillStyle = fillStyle;
    Element element = element_make(ELEMENT_RECTANGLE, &style);
    element.from = (Point){20.0f, 20.0f};
    element.to = (Point){180.0f, 180.0f};
    RECT clip = canvas_area(&canvas);
    render_element(&canvas, &element, &clip);
    *empty = 0;
    *inked = 0;
    for (int x = 40; x < 160; x++) {
        uint32_t alpha = canvas.pixels[100 * CANVAS_SIZE + x] >> 24;
        *empty += alpha == 0;
        *inked += alpha != 0;
    }
    canvas_destroy(&canvas);
}

/**
 * @brief Solid fills cover the interior; hachure and cross-hatch leave gaps between lines.
 */
static void test_fill_styles_render(void)
{
    int empty;
    int inked;
    scan_fill_row(FILL_STYLE_SOLID, &empty, &inked);
    assert(empty == 0);
    scan_fill_row(FILL_STYLE_HACHURE, &empty, &inked);
    assert(empty > 0 && inked > 0);
    scan_fill_row(FILL_STYLE_CROSS_HATCH, &empty, &inked);
    assert(empty > 0 && inked > 0);
}

/**
 * @brief Renders a horizontal line and reports whether pixels exist where an end arrowhead wing would be.
 * @param heads Arrowhead style of the line.
 * @param endWing Receives true when the wing beside the end point is inked.
 * @param startWing Receives true when the wing beside the start point is inked.
 */
static void line_wings(ArrowHeads heads, bool *endWing, bool *startWing)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_SIZE, CANVAS_SIZE));
    Style style = style_default();
    style.arrowHeads = heads;
    Element element = element_make(ELEMENT_LINE, &style);
    element.from = (Point){20.0f, 100.0f};
    element.to = (Point){180.0f, 100.0f};
    RECT clip = canvas_area(&canvas);
    render_element(&canvas, &element, &clip);
    *endWing = (canvas.pixels[96 * CANVAS_SIZE + 170] >> 24) != 0;
    *startWing = (canvas.pixels[96 * CANVAS_SIZE + 30] >> 24) != 0;
    canvas_destroy(&canvas);
}

/**
 * @brief A line draws no head, an end head or both heads according to its arrowhead style.
 */
static void test_line_arrowheads(void)
{
    bool end;
    bool start;
    line_wings(ARROW_HEAD_NONE, &end, &start);
    assert(!end && !start);
    line_wings(ARROW_HEAD_END, &end, &start);
    assert(end && !start);
    line_wings(ARROW_HEAD_BOTH, &end, &start);
    assert(end && start);
}

/**
 * @brief A shape with text draws ink in the middle of its box, and its bounds include the label.
 */
static void test_shape_label(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_SIZE, CANVAS_SIZE));
    Style style = style_default();
    style.fontSize = 36;
    Element element = element_make(ELEMENT_ELLIPSE, &style);
    element.from = (Point){20.0f, 20.0f};
    element.to = (Point){180.0f, 180.0f};
    assert(element_set_text(&element, L"WWWW"));
    RECT clip = canvas_area(&canvas);
    render_element(&canvas, &element, &clip);
    int inked = 0;
    for (int y = 80; y < 120; y++) {
        for (int x = 60; x < 140; x++) {
            inked += (canvas.pixels[y * CANVAS_SIZE + x] >> 24) != 0;
        }
    }
    assert(inked > 50);
    Element small = element_make(ELEMENT_RECTANGLE, &style);
    small.from = (Point){90.0f, 90.0f};
    small.to = (Point){110.0f, 110.0f};
    RECT withoutLabel = element_bounds(&small);
    assert(element_set_text(&small, L"A long label that overflows the shape"));
    RECT withLabel = element_bounds(&small);
    assert(withLabel.left < withoutLabel.left && withLabel.right > withoutLabel.right);
    element_free(&element);
    element_free(&small);
    canvas_destroy(&canvas);
}

int main(void)
{
    test_shape_label();
    test_line_arrowheads();
    test_rectangle_corners();
    test_fill_styles_render();
    printf("render tests passed\n");
    return 0;
}
