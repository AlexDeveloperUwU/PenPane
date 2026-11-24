/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>

#include "../src/hittest.h"

/**
 * @brief Builds a two-point element with the default style.
 * @param type Element kind.
 * @param x1 First point x.
 * @param y1 First point y.
 * @param x2 Second point x.
 * @param y2 Second point y.
 * @return Element owning no memory.
 */
static Element make_shape(ElementType type, float x1, float y1, float x2, float y2)
{
    Style style = style_default();
    style.fill = 0;
    Element element = element_make(type, &style);
    element.from = (Point){x1, y1};
    element.to = (Point){x2, y2};
    return element;
}

/**
 * @brief Unfilled ellipses hit on the outline only; filled ones hit inside as well.
 */
static void test_ellipse(void)
{
    Element ellipse = make_shape(ELEMENT_ELLIPSE, 100, 100, 300, 200);
    assert(hit_element(&ellipse, (Point){100, 150}, 4.0f));
    assert(hit_element(&ellipse, (Point){200, 100}, 4.0f));
    assert(!hit_element(&ellipse, (Point){200, 150}, 4.0f));
    assert(!hit_element(&ellipse, (Point){110, 105}, 4.0f));
    ellipse.style.fill = COLOR_FILL_BLUE;
    assert(hit_element(&ellipse, (Point){200, 150}, 4.0f));
    assert(!hit_element(&ellipse, (Point){110, 105}, 4.0f));
}

/**
 * @brief Diamonds hit on their four edges; filled ones inside, but not in the corners of the box.
 */
static void test_diamond(void)
{
    Element diamond = make_shape(ELEMENT_DIAMOND, 100, 100, 300, 200);
    assert(hit_element(&diamond, (Point){200, 100}, 4.0f));
    assert(hit_element(&diamond, (Point){300, 150}, 4.0f));
    assert(hit_element(&diamond, (Point){150, 125}, 4.0f));
    assert(!hit_element(&diamond, (Point){200, 150}, 4.0f));
    assert(!hit_element(&diamond, (Point){105, 105}, 4.0f));
    diamond.style.fill = COLOR_FILL_BLUE;
    assert(hit_element(&diamond, (Point){200, 150}, 4.0f));
    assert(!hit_element(&diamond, (Point){105, 105}, 4.0f));
}

/**
 * @brief Lines and arrows hit within half the stroke width plus the tolerance of the segment.
 */
static void test_line_and_arrow(void)
{
    ElementType types[2] = {ELEMENT_LINE, ELEMENT_ARROW};
    for (int i = 0; i < 2; i++) {
        Element line = make_shape(types[i], 100, 100, 300, 100);
        assert(hit_element(&line, (Point){200, 100}, 4.0f));
        assert(hit_element(&line, (Point){200, 103}, 4.0f));
        assert(!hit_element(&line, (Point){200, 130}, 4.0f));
        assert(!hit_element(&line, (Point){320, 100}, 4.0f));
    }
}

/**
 * @brief Pen strokes hit near any segment, including a single-point dot.
 */
static void test_pen(void)
{
    Style style = style_default();
    Element stroke = element_make(ELEMENT_PEN, &style);
    assert(element_add_point(&stroke, (PenPoint){100, 100, 0.5f}));
    assert(hit_element(&stroke, (Point){100, 100}, 4.0f));
    assert(!hit_element(&stroke, (Point){150, 100}, 4.0f));
    assert(element_add_point(&stroke, (PenPoint){200, 100, 0.5f}));
    assert(element_add_point(&stroke, (PenPoint){200, 200, 0.5f}));
    assert(hit_element(&stroke, (Point){150, 100}, 4.0f));
    assert(hit_element(&stroke, (Point){200, 150}, 4.0f));
    assert(!hit_element(&stroke, (Point){100, 200}, 4.0f));
    element_free(&stroke);
}

/**
 * @brief Handles are found at the corners and edge midpoints of the selection box, in clockwise order.
 */
static void test_handles(void)
{
    Element rectangle = make_shape(ELEMENT_RECTANGLE, 100, 100, 300, 200);
    FloatRect box = element_selection_box(&rectangle);
    float middleX = (box.left + box.right) * 0.5f;
    float middleY = (box.top + box.bottom) * 0.5f;
    assert(hit_handle(&rectangle, (Point){box.left, box.top}) == 0);
    assert(hit_handle(&rectangle, (Point){middleX, box.top}) == 1);
    assert(hit_handle(&rectangle, (Point){box.right, box.top}) == 2);
    assert(hit_handle(&rectangle, (Point){box.right, middleY}) == 3);
    assert(hit_handle(&rectangle, (Point){box.right, box.bottom}) == 4);
    assert(hit_handle(&rectangle, (Point){middleX, box.bottom}) == 5);
    assert(hit_handle(&rectangle, (Point){box.left, box.bottom}) == 6);
    assert(hit_handle(&rectangle, (Point){box.left, middleY}) == 7);
    assert(hit_handle(&rectangle, (Point){middleX, middleY}) == HIT_HANDLE_NONE);
    Element text = make_shape(ELEMENT_TEXT, 100, 100, 200, 130);
    assert(hit_handle(&text, (Point){100, 100}) == HIT_HANDLE_NONE);
}

int main(void)
{
    test_ellipse();
    test_diamond();
    test_line_and_arrow();
    test_pen();
    test_handles();
    printf("hittest tests passed\n");
    return 0;
}
