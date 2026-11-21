#include "element.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "text.h"

#define ARROWHEAD_SIZE 25.0f
#define ELEMENT_BOUNDS_MARGIN 3.0f
#define PEN_MAX_RADIUS_FACTOR 12.0f

Element element_make(ElementType type, const Style *style)
{
    Element element;
    memset(&element, 0, sizeof element);
    element.type = type;
    element.style = *style;
    return element;
}

bool element_copy(Element *destination, const Element *source)
{
    *destination = *source;
    destination->points = NULL;
    destination->text = NULL;
    destination->pointCapacity = 0;
    if (source->pointCount > 0) {
        destination->points = malloc(sizeof(PenPoint) * (size_t)source->pointCount);
        if (!destination->points) {
            destination->pointCount = 0;
            return false;
        }
        memcpy(destination->points, source->points, sizeof(PenPoint) * (size_t)source->pointCount);
        destination->pointCapacity = source->pointCount;
    }
    if (source->text) {
        destination->text = _wcsdup(source->text);
        if (!destination->text) {
            free(destination->points);
            destination->points = NULL;
            destination->pointCount = 0;
            return false;
        }
    }
    return true;
}

void element_free(Element *element)
{
    free(element->points);
    free(element->text);
    memset(element, 0, sizeof *element);
}

bool element_add_point(Element *element, PenPoint point)
{
    if (element->pointCount == element->pointCapacity) {
        int capacity = element->pointCapacity ? element->pointCapacity * 2 : 64;
        PenPoint *grown = realloc(element->points, sizeof(PenPoint) * (size_t)capacity);
        if (!grown) {
            return false;
        }
        element->points = grown;
        element->pointCapacity = capacity;
    }
    element->points[element->pointCount++] = point;
    return true;
}

bool element_set_text(Element *element, const wchar_t *text)
{
    wchar_t *copy = _wcsdup(text);
    if (!copy) {
        return false;
    }
    free(element->text);
    element->text = copy;
    return true;
}

FloatRect element_box(const Element *element)
{
    FloatRect box;
    switch (element->type) {
    case ELEMENT_PEN:
        if (element->pointCount == 0) {
            return (FloatRect){0, 0, 0, 0};
        }
        box = (FloatRect){element->points[0].x, element->points[0].y, element->points[0].x,
                          element->points[0].y};
        for (int i = 1; i < element->pointCount; i++) {
            const PenPoint *p = &element->points[i];
            box.left = p->x < box.left ? p->x : box.left;
            box.right = p->x > box.right ? p->x : box.right;
            box.top = p->y < box.top ? p->y : box.top;
            box.bottom = p->y > box.bottom ? p->y : box.bottom;
        }
        return box;
    case ELEMENT_TEXT: {
        int width = 0;
        int height = 0;
        text_measure(TEXT_FONT_CANVAS, element->text ? element->text : L"", element->fontSize,
                     &width, &height);
        return (FloatRect){element->from.x, element->from.y, element->from.x + (float)width,
                           element->from.y + (float)height};
    }
    default:
        return float_rect_from_points(element->from, element->to);
    }
}

RECT element_bounds(const Element *element)
{
    float margin = element->style.width + ELEMENT_BOUNDS_MARGIN;
    if (element->type == ELEMENT_ARROW) {
        margin += ARROWHEAD_SIZE;
    } else if (element->type == ELEMENT_PEN) {
        margin = element->style.width * PEN_MAX_RADIUS_FACTOR + ELEMENT_BOUNDS_MARGIN;
    } else if (element->type == ELEMENT_TEXT) {
        margin = 4.0f;
    }
    return rect_from_float(element_box(element), margin);
}

void element_translate(Element *element, float dx, float dy)
{
    element->from.x += dx;
    element->from.y += dy;
    element->to.x += dx;
    element->to.y += dy;
    for (int i = 0; i < element->pointCount; i++) {
        element->points[i].x += dx;
        element->points[i].y += dy;
    }
}

/**
 * @brief Maps one coordinate between boxes along one axis.
 * @param value Coordinate to map.
 * @param oldMin Old box minimum.
 * @param oldMax Old box maximum.
 * @param newMin New box minimum.
 * @param newMax New box maximum.
 * @return Mapped coordinate; collapses to newMin when the old extent is zero.
 */
static float remap_axis(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    float extent = oldMax - oldMin;
    if (fabsf(extent) < 1e-4f) {
        return newMin;
    }
    return newMin + (value - oldMin) / extent * (newMax - newMin);
}

void element_remap(Element *element, FloatRect oldBox, FloatRect newBox)
{
    element->from.x = remap_axis(element->from.x, oldBox.left, oldBox.right, newBox.left, newBox.right);
    element->from.y = remap_axis(element->from.y, oldBox.top, oldBox.bottom, newBox.top, newBox.bottom);
    element->to.x = remap_axis(element->to.x, oldBox.left, oldBox.right, newBox.left, newBox.right);
    element->to.y = remap_axis(element->to.y, oldBox.top, oldBox.bottom, newBox.top, newBox.bottom);
    for (int i = 0; i < element->pointCount; i++) {
        element->points[i].x =
            remap_axis(element->points[i].x, oldBox.left, oldBox.right, newBox.left, newBox.right);
        element->points[i].y =
            remap_axis(element->points[i].y, oldBox.top, oldBox.bottom, newBox.top, newBox.bottom);
    }
}

bool element_is_filled(const Element *element)
{
    return (element->type == ELEMENT_RECTANGLE || element->type == ELEMENT_ELLIPSE) &&
           (element->style.fill >> 24) != 0;
}
