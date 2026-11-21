/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hittest.h"

#include <math.h>

#include "pen.h"
#include "render.h"

#define HANDLE_HIT_RADIUS 10.0f

/**
 * @brief Distance from a point to the boundary of a rectangle.
 * @param point Query point.
 * @param box Rectangle.
 * @return Distance to the nearest edge.
 */
static float rectangle_edge_distance(Point point, FloatRect box)
{
    Point a = {box.left, box.top};
    Point b = {box.right, box.top};
    Point c = {box.right, box.bottom};
    Point d = {box.left, box.bottom};
    float best = segment_distance(point, a, b);
    float next = segment_distance(point, b, c);
    best = next < best ? next : best;
    next = segment_distance(point, c, d);
    best = next < best ? next : best;
    next = segment_distance(point, d, a);
    return next < best ? next : best;
}

/**
 * @brief Tests whether a point lies inside a box.
 * @param point Query point.
 * @param box Rectangle.
 * @return true when inside or on the border.
 */
static bool inside_box(Point point, FloatRect box)
{
    return point.x >= box.left && point.x <= box.right && point.y >= box.top && point.y <= box.bottom;
}

/**
 * @brief Approximate distance from a point to an ellipse outline.
 * @param point Query point.
 * @param box Ellipse bounding box.
 * @return Distance in pixels, accurate near the boundary.
 */
static float ellipse_edge_distance(Point point, FloatRect box)
{
    float cx = (box.left + box.right) * 0.5f;
    float cy = (box.top + box.bottom) * 0.5f;
    float rx = (box.right - box.left) * 0.5f;
    float ry = (box.bottom - box.top) * 0.5f;
    if (rx < 0.5f || ry < 0.5f) {
        return segment_distance(point, (Point){box.left, box.top}, (Point){box.right, box.bottom});
    }
    float nx = (point.x - cx) / rx;
    float ny = (point.y - cy) / ry;
    float k0 = sqrtf(nx * nx + ny * ny);
    float gx = nx / rx;
    float gy = ny / ry;
    float k1 = sqrtf(gx * gx + gy * gy);
    if (k1 < 1e-9f) {
        return rx < ry ? rx : ry;
    }
    return fabsf(k0 * (k0 - 1.0f) / k1);
}

bool hit_element(const Element *element, Point point, float tolerance)
{
    FloatRect box = element_box(element);
    float reach = element->style.width * 0.5f + tolerance;
    switch (element->type) {
    case ELEMENT_RECTANGLE:
        if (element_is_filled(element) && inside_box(point, box)) {
            return true;
        }
        return rectangle_edge_distance(point, box) <= reach;
    case ELEMENT_ELLIPSE: {
        float distance = ellipse_edge_distance(point, box);
        if (element_is_filled(element)) {
            float cx = (box.left + box.right) * 0.5f;
            float cy = (box.top + box.bottom) * 0.5f;
            float nx = (point.x - cx) / ((box.right - box.left) * 0.5f + 0.001f);
            float ny = (point.y - cy) / ((box.bottom - box.top) * 0.5f + 0.001f);
            if (nx * nx + ny * ny <= 1.0f) {
                return true;
            }
        }
        return distance <= reach;
    }
    case ELEMENT_ARROW:
    case ELEMENT_LINE:
        return segment_distance(point, element->from, element->to) <= reach;
    case ELEMENT_PEN: {
        float radius = pen_radius(element->style.width, 0.7f) + tolerance;
        if (element->pointCount == 1) {
            Point only = {element->points[0].x, element->points[0].y};
            return segment_distance(point, only, only) <= radius;
        }
        for (int i = 0; i + 1 < element->pointCount; i++) {
            Point a = {element->points[i].x, element->points[i].y};
            Point b = {element->points[i + 1].x, element->points[i + 1].y};
            if (segment_distance(point, a, b) <= radius) {
                return true;
            }
        }
        return false;
    }
    case ELEMENT_TEXT:
        return inside_box(point, (FloatRect){box.left - tolerance, box.top - tolerance,
                                             box.right + tolerance, box.bottom + tolerance});
    }
    return false;
}

int hit_handle(const Element *element, Point point)
{
    if (element->type == ELEMENT_TEXT) {
        return HIT_HANDLE_NONE;
    }
    FloatRect box = render_selection_box(element);
    float xs[3] = {box.left, (box.left + box.right) * 0.5f, box.right};
    float ys[3] = {box.top, (box.top + box.bottom) * 0.5f, box.bottom};
    static const int ORDER[HIT_HANDLE_COUNT][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1},
                                                   {2, 2}, {1, 2}, {0, 2}, {0, 1}};
    for (int i = 0; i < HIT_HANDLE_COUNT; i++) {
        float dx = point.x - xs[ORDER[i][0]];
        float dy = point.y - ys[ORDER[i][1]];
        if (dx * dx + dy * dy <= HANDLE_HIT_RADIUS * HANDLE_HIT_RADIUS) {
            return i;
        }
    }
    return HIT_HANDLE_NONE;
}
