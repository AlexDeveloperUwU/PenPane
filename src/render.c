/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "render.h"

#include <math.h>
#include <string.h>

#include "mask_scratch.h"
#include "pen.h"
#include "text.h"

#define PI_FLOAT 3.14159265f
#define ARROWHEAD_SIZE 25.0f
#define ARROWHEAD_MIN_ANGLE 20.0f
#define ARROWHEAD_MAX_ANGLE 30.0f
#define ERASING_OPACITY 0.2f
#define CORNER_RATIO 0.25f
#define CORNER_MAX 32.0f
#define MIN_VISIBLE_WIDTH 1.5f
#define DASH_ON 8.0f
#define DASH_OFF 8.0f
#define DOT_ON 1.5f
#define DOT_OFF 6.0f
#define CORNER_STEPS 12
#define RECT_PATH_POINTS 64
#define MAX_PATH_POINTS 257
#define ELLIPSE_SEGMENT_LENGTH 6.0f
#define ELLIPSE_MIN_STEPS 48
#define HANDLE_SIZE 8.0f
#define HANDLE_RADIUS 2.0f
#define SELECTION_PADDING 3.5f
#define SELECTION_OUTLINE_WIDTH 1.0f
#define SELECTION_COLOR COLOR_SELECTION

/** @brief Pen segments forwarded into a mask. */
typedef struct MaskSink {
    Mask *mask;
} MaskSink;

/**
 * @brief Pen segment callback that adds a capsule to a mask.
 * @param user MaskSink pointer.
 * @param from Segment start.
 * @param to Segment end.
 * @param radiusFrom Start radius.
 * @param radiusTo End radius.
 */
static void sink_capsule(void *user, Point from, Point to, float radiusFrom, float radiusTo)
{
    mask_add_capsule(((MaskSink *)user)->mask, from, to, radiusFrom, radiusTo);
}

/**
 * @brief Applies an element's opacity (and erasing preview) to a color.
 * @param color Straight-alpha color.
 * @param element Owning element.
 * @return Color with alpha scaled.
 */
static Color with_opacity(Color color, const Element *element)
{
    float factor = element->style.opacity * (element->erasing ? ERASING_OPACITY : 1.0f) *
                   (element->style.highlighter ? HIGHLIGHTER_ALPHA : 1.0f);
    uint32_t alpha = (uint32_t)((float)(color >> 24) * factor + 0.5f);
    return (color & 0x00FFFFFFu) | (alpha << 24);
}

/**
 * @brief Width at which a stroke is actually drawn; Excalidraw renders its thin preset 1.5 pixels wide.
 * @param width Nominal stroke width preset.
 * @return Drawn stroke width in pixels.
 */
static float visible_width(float width)
{
    return width < MIN_VISIBLE_WIDTH ? MIN_VISIBLE_WIDTH : width;
}

/**
 * @brief Corner radius used for rectangles, matching Excalidraw's proportional roundness.
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @return Radius in pixels.
 */
static float corner_radius(float width, float height)
{
    float shorter = width < height ? width : height;
    float radius = shorter * CORNER_RATIO;
    return radius > CORNER_MAX ? CORNER_MAX : radius;
}

/**
 * @brief Appends the outline of a rounded rectangle to a point list.
 * @param points Output points.
 * @param box Rectangle box.
 * @param radius Corner radius.
 * @return Number of points written; the path is closed by repeating the first point.
 */
static int rect_path(Point *points, FloatRect box, float radius)
{
    int count = 0;
    Point corners[4] = {{box.right, box.top}, {box.right, box.bottom}, {box.left, box.bottom},
                        {box.left, box.top}};
    Point entry[4] = {{box.right - radius, box.top}, {box.right, box.bottom - radius},
                      {box.left + radius, box.bottom}, {box.left, box.top + radius}};
    Point exit[4] = {{box.right, box.top + radius}, {box.right - radius, box.bottom},
                     {box.left, box.bottom - radius}, {box.left + radius, box.top}};
    for (int corner = 0; corner < 4; corner++) {
        for (int step = 0; step <= CORNER_STEPS; step++) {
            float t = (float)step / (float)CORNER_STEPS;
            float u = 1.0f - t;
            points[count].x = u * u * entry[corner].x + 2.0f * u * t * corners[corner].x +
                              t * t * exit[corner].x;
            points[count].y = u * u * entry[corner].y + 2.0f * u * t * corners[corner].y +
                              t * t * exit[corner].y;
            count++;
        }
    }
    points[count++] = points[0];
    return count;
}

/**
 * @brief Samples an ellipse outline into a closed point list.
 * @param points Output points.
 * @param box Ellipse bounding box.
 * @return Number of points written; the path is closed.
 */
static int ellipse_path(Point *points, FloatRect box)
{
    float cx = (box.left + box.right) * 0.5f;
    float cy = (box.top + box.bottom) * 0.5f;
    float rx = (box.right - box.left) * 0.5f;
    float ry = (box.bottom - box.top) * 0.5f;
    float perimeter = PI_FLOAT * (3.0f * (rx + ry) - sqrtf((3.0f * rx + ry) * (rx + 3.0f * ry)));
    int steps = (int)ceilf(perimeter / ELLIPSE_SEGMENT_LENGTH);
    steps = steps < ELLIPSE_MIN_STEPS ? ELLIPSE_MIN_STEPS : (steps > MAX_PATH_POINTS - 1 ? MAX_PATH_POINTS - 1 : steps);
    for (int i = 0; i < steps; i++) {
        float angle = (float)i / (float)steps * 2.0f * PI_FLOAT;
        points[i].x = cx + cosf(angle) * rx;
        points[i].y = cy + sinf(angle) * ry;
    }
    points[steps] = points[0];
    return steps + 1;
}

/**
 * @brief Strokes a polyline with a dash pattern into a mask.
 * @param mask Target mask.
 * @param points Polyline points.
 * @param count Number of points.
 * @param width Stroke width.
 * @param style Dashed or dotted pattern.
 */
static void stroke_dashed(Mask *mask, const Point *points, int count, float width, StrokeStyle style)
{
    float on = style == STROKE_DASHED ? DASH_ON : DOT_ON;
    float off = (style == STROKE_DASHED ? DASH_OFF : DOT_OFF) + width;
    float radius = visible_width(width) * 0.5f;
    float remaining = on;
    bool drawing = true;
    for (int i = 0; i + 1 < count; i++) {
        Point from = points[i];
        float dx = points[i + 1].x - from.x;
        float dy = points[i + 1].y - from.y;
        float length = sqrtf(dx * dx + dy * dy);
        float consumed = 0.0f;
        while (consumed < length) {
            float step = remaining < length - consumed ? remaining : length - consumed;
            if (drawing) {
                Point a = {from.x + dx * (consumed / length), from.y + dy * (consumed / length)};
                Point b = {from.x + dx * ((consumed + step) / length),
                           from.y + dy * ((consumed + step) / length)};
                mask_add_capsule(mask, a, b, radius, radius);
            }
            consumed += step;
            remaining -= step;
            if (remaining <= 0.0f) {
                drawing = !drawing;
                remaining = drawing ? on : off;
            }
        }
    }
}

/**
 * @brief Strokes a polyline solidly into a mask.
 * @param mask Target mask.
 * @param points Polyline points.
 * @param count Number of points.
 * @param width Stroke width.
 */
static void stroke_solid(Mask *mask, const Point *points, int count, float width)
{
    float radius = visible_width(width) * 0.5f;
    for (int i = 0; i + 1 < count; i++) {
        mask_add_capsule(mask, points[i], points[i + 1], radius, radius);
    }
}

/**
 * @brief Strokes a polyline in the element's style.
 * @param mask Target mask.
 * @param points Polyline points.
 * @param count Number of points.
 * @param style Element style.
 */
static void stroke_path(Mask *mask, const Point *points, int count, const Style *style)
{
    if (style->strokeStyle == STROKE_SOLID) {
        stroke_solid(mask, points, count, style->width);
    } else {
        stroke_dashed(mask, points, count, style->width, style->strokeStyle);
    }
}

/**
 * @brief Adds an open V arrowhead at the end of a line.
 * @param mask Target mask.
 * @param from Line start.
 * @param to Line end where the tip sits.
 * @param width Stroke width.
 */
static void add_arrowhead(Mask *mask, Point from, Point to, float width)
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 1.0f) {
        return;
    }
    float size = ARROWHEAD_SIZE;
    float limit = length * 0.5f;
    size = size < limit ? size : limit;
    float shrink = (ARROWHEAD_SIZE - size) / 15.0f;
    shrink = shrink > 1.0f ? 1.0f : shrink;
    float angle = (ARROWHEAD_MIN_ANGLE + (ARROWHEAD_MAX_ANGLE - ARROWHEAD_MIN_ANGLE) * shrink) * PI_FLOAT / 180.0f;
    float ux = -dx / length;
    float uy = -dy / length;
    float radius = width * 0.5f;
    for (int side = -1; side <= 1; side += 2) {
        float a = (float)side * angle;
        Point wing = {to.x + (ux * cosf(a) - uy * sinf(a)) * size,
                      to.y + (ux * sinf(a) + uy * cosf(a)) * size};
        mask_add_capsule(mask, to, wing, radius, radius);
    }
}

/**
 * @brief Renders a rectangle element.
 * @param canvas Target canvas.
 * @param element Rectangle element.
 * @param area Pixel work area.
 */
static void render_rectangle(Canvas *canvas, const Element *element, RECT area)
{
    FloatRect box = element_box(element);
    float width = box.right - box.left;
    float height = box.bottom - box.top;
    float radius = corner_radius(width, height);
    Point points[RECT_PATH_POINTS];
    int count = rect_path(points, box, radius);
    Mask mask;
    if (element_is_filled(element) && mask_scratch_acquire(&mask, area)) {
        mask_add_polygon_fill(&mask, points, count - 1);
        mask_blend(canvas, &mask, with_opacity(element->style.fill, element));
    }
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    stroke_path(&mask, points, count, &element->style);
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
}

/**
 * @brief Renders an ellipse element.
 * @param canvas Target canvas.
 * @param element Ellipse element.
 * @param area Pixel work area.
 */
static void render_ellipse(Canvas *canvas, const Element *element, RECT area)
{
    FloatRect box = element_box(element);
    Point center = {(box.left + box.right) * 0.5f, (box.top + box.bottom) * 0.5f};
    float rx = (box.right - box.left) * 0.5f;
    float ry = (box.bottom - box.top) * 0.5f;
    Mask mask;
    if (rx < 0.5f || ry < 0.5f) {
        return;
    }
    if (element_is_filled(element) && mask_scratch_acquire(&mask, area)) {
        mask_add_ellipse_fill(&mask, center, rx, ry);
        mask_blend(canvas, &mask, with_opacity(element->style.fill, element));
    }
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    if (element->style.strokeStyle == STROKE_SOLID) {
        mask_add_ellipse_stroke(&mask, center, rx, ry, visible_width(element->style.width));
    } else {
        Point points[MAX_PATH_POINTS];
        int count = ellipse_path(points, box);
        stroke_path(&mask, points, count, &element->style);
    }
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
}

/**
 * @brief Renders a line or an arrow element.
 * @param canvas Target canvas.
 * @param element Line or arrow element.
 * @param area Pixel work area.
 */
static void render_line(Canvas *canvas, const Element *element, RECT area)
{
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    Point points[2] = {element->from, element->to};
    stroke_path(&mask, points, 2, &element->style);
    if (element->type == ELEMENT_ARROW) {
        float width = visible_width(element->style.width);
        add_arrowhead(&mask, element->from, element->to, width);
        if (element->style.arrowHeads == ARROW_HEAD_BOTH) {
            add_arrowhead(&mask, element->to, element->from, width);
        }
    }
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
}

/**
 * @brief Renders a diamond element: the four edge midpoints of its box joined by straight lines.
 * @param canvas Target canvas.
 * @param element Diamond element.
 * @param area Pixel work area.
 */
static void render_diamond(Canvas *canvas, const Element *element, RECT area)
{
    FloatRect box = element_box(element);
    float cx = (box.left + box.right) * 0.5f;
    float cy = (box.top + box.bottom) * 0.5f;
    Point points[5] = {{cx, box.top}, {box.right, cy}, {cx, box.bottom}, {box.left, cy}, {cx, box.top}};
    Mask mask;
    if (element_is_filled(element) && mask_scratch_acquire(&mask, area)) {
        mask_add_polygon_fill(&mask, points, 4);
        mask_blend(canvas, &mask, with_opacity(element->style.fill, element));
    }
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    stroke_path(&mask, points, 5, &element->style);
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
}

/**
 * @brief Renders a pen element.
 * @param canvas Target canvas.
 * @param element Pen element.
 * @param area Pixel work area.
 */
static void render_pen(Canvas *canvas, const Element *element, RECT area)
{
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    MaskSink sink = {&mask};
    pen_emit(element, 0, sink_capsule, &sink);
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
}

/**
 * @brief Renders a text element.
 * @param canvas Target canvas.
 * @param element Text element.
 * @param area Pixel work area.
 */
static void render_text(Canvas *canvas, const Element *element, RECT area)
{
    if (!element->text || !element->text[0]) {
        return;
    }
    Mask mask;
    if (!text_make_mask(&mask, text_font_for_face(element->style.face), element->text, element->fontSize,
                        (int)element->from.x, (int)element->from.y)) {
        return;
    }
    RECT textArea = {mask.x, mask.y, mask.x + mask.width, mask.y + mask.height};
    RECT visible;
    if (rect_intersect(textArea, area, &visible)) {
        mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
    }
    mask_destroy(&mask);
}

void render_element(Canvas *canvas, const Element *element, const RECT *clip)
{
    RECT canvasRect = canvas_area(canvas);
    RECT area;
    RECT bounds = element_bounds(element);
    if (!rect_intersect(bounds, *clip, &area) || !rect_intersect(area, canvasRect, &area)) {
        return;
    }
    switch (element->type) {
    case ELEMENT_RECTANGLE:
        render_rectangle(canvas, element, area);
        break;
    case ELEMENT_ELLIPSE:
        render_ellipse(canvas, element, area);
        break;
    case ELEMENT_DIAMOND:
        render_diamond(canvas, element, area);
        break;
    case ELEMENT_ARROW:
    case ELEMENT_LINE:
        render_line(canvas, element, area);
        break;
    case ELEMENT_PEN:
        render_pen(canvas, element, area);
        break;
    case ELEMENT_TEXT:
        render_text(canvas, element, area);
        break;
    }
}

void render_scene(Canvas *canvas, const Scene *scene, const RECT *clip, Color backdrop)
{
    RECT canvasRect = canvas_area(canvas);
    RECT region;
    if (!rect_intersect(*clip, canvasRect, &region)) {
        return;
    }
    canvas_fill(canvas, &region, backdrop);
    for (int i = 0; i < scene->count; i++) {
        render_element(canvas, &scene->items[i], &region);
    }
}

RECT render_pen_tail(Canvas *canvas, const Element *element, int firstGroup, const RECT *clip)
{
    int count = element->pointCount;
    int first = firstGroup < 0 ? 0 : firstGroup;
    int from = first > 0 ? first - 1 : 0;
    FloatRect box = {1e9f, 1e9f, -1e9f, -1e9f};
    for (int i = from; i < count; i++) {
        const PenPoint *p = &element->points[i];
        box.left = p->x < box.left ? p->x : box.left;
        box.right = p->x > box.right ? p->x : box.right;
        box.top = p->y < box.top ? p->y : box.top;
        box.bottom = p->y > box.bottom ? p->y : box.bottom;
    }
    RECT touched = {0, 0, 0, 0};
    if (count == 0) {
        return touched;
    }
    touched = rect_from_float(box, element->style.width * 12.0f + 3.0f);
    RECT area;
    RECT canvasRect = canvas_area(canvas);
    if (!rect_intersect(touched, *clip, &area) || !rect_intersect(area, canvasRect, &area)) {
        return touched;
    }
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return touched;
    }
    MaskSink sink = {&mask};
    pen_emit(element, first > 0 ? first - 1 : 0, sink_capsule, &sink);
    mask_blend(canvas, &mask, with_opacity(element->style.stroke, element));
    return touched;
}

/**
 * @brief Computes the selection box in pixels, padded around the element geometry.
 * @param element Selected element.
 * @return Padded floating rectangle.
 */
FloatRect render_selection_box(const Element *element)
{
    FloatRect box = element_box(element);
    float pad = SELECTION_PADDING + element->style.width * 0.5f;
    box.left -= pad;
    box.top -= pad;
    box.right += pad;
    box.bottom += pad;
    return box;
}

RECT render_selection_bounds(const Element *element)
{
    return rect_from_float(render_selection_box(element), HANDLE_SIZE);
}

void render_selection(Canvas *canvas, const Element *element, const RECT *clip)
{
    RECT bounds = render_selection_bounds(element);
    RECT area;
    if (!rect_intersect(bounds, *clip, &area)) {
        return;
    }
    Mask mask;
    if (!mask_scratch_acquire(&mask, area)) {
        return;
    }
    FloatRect box = render_selection_box(element);
    box.left = roundf(box.left);
    box.top = roundf(box.top);
    box.right = roundf(box.right);
    box.bottom = roundf(box.bottom);
    mask_add_rect_stroke(&mask, box.left, box.top, box.right - box.left, box.bottom - box.top, 0.0f,
                         SELECTION_OUTLINE_WIDTH);
    mask_blend(canvas, &mask, SELECTION_COLOR);
    float xs[2] = {box.left, box.right};
    float ys[2] = {box.top, box.bottom};
    for (int row = 0; row < 2; row++) {
        for (int column = 0; column < 2; column++) {
            float left = xs[column] - HANDLE_SIZE * 0.5f;
            float top = ys[row] - HANDLE_SIZE * 0.5f;
            if (!mask_scratch_acquire(&mask, area)) {
                return;
            }
            mask_add_rect_fill(&mask, left, top, HANDLE_SIZE, HANDLE_SIZE, HANDLE_RADIUS);
            mask_blend(canvas, &mask, COLOR_CANVAS_DARK);
            if (!mask_scratch_acquire(&mask, area)) {
                return;
            }
            mask_add_rect_stroke(&mask, left, top, HANDLE_SIZE, HANDLE_SIZE, HANDLE_RADIUS,
                                 SELECTION_OUTLINE_WIDTH);
            mask_blend(canvas, &mask, SELECTION_COLOR);
        }
    }
}

void render_text_caret(Canvas *canvas, const Element *element, const RECT *clip)
{
    int width = 0;
    int height = 0;
    const wchar_t *text = element->text ? element->text : L"";
    const wchar_t *lastLine = wcsrchr(text, L'\n');
    lastLine = lastLine ? lastLine + 1 : text;
    TextFont font = text_font_for_face(element->style.face);
    text_measure(font, text, element->fontSize, &width, &height);
    int lineWidth = 0;
    int lineHeight = 0;
    text_measure(font, lastLine, element->fontSize, &lineWidth, &lineHeight);
    int lineTop = (int)element->from.y + height - lineHeight;
    Point top = {element->from.x + (float)lineWidth + 1.0f, (float)lineTop + 2.0f};
    Point bottom = {top.x, (float)(lineTop + (lineHeight > 4 ? lineHeight - 2 : lineHeight))};
    RECT area = {(int)top.x - 3, lineTop - 2, (int)top.x + 4, lineTop + lineHeight + 2};
    RECT visible;
    if (!rect_intersect(area, *clip, &visible)) {
        return;
    }
    Mask mask;
    if (!mask_scratch_acquire(&mask, visible)) {
        return;
    }
    mask_add_capsule(&mask, top, bottom, 1.0f, 1.0f);
    mask_blend(canvas, &mask, element->style.stroke);
}
