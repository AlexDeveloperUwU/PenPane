/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "icons.h"

#include <math.h>

#include "shapes_ui.h"
#include "tool.h"

#define ICON_GRID 16.0f
#define ICON_HALF_STROKE 0.7f
#define ICON_MAX_VERTICES 16
#define PI_FLOAT 3.14159265f

/** @brief Drawing context mapping the 16 unit grid onto a scratch mask. */
typedef struct Pen {
    Mask *mask;
    float originX;
    float originY;
    float unit;
    float half;
    bool mirror;
    float angle;
} Pen;

/**
 * @brief Maps grid coordinates to mask coordinates, mirroring horizontally and rotating around the
 *        center of the grid when requested.
 * @param pen Drawing context.
 * @param u Grid x.
 * @param v Grid y.
 * @return Point in canvas pixels.
 */
static Point at(const Pen *pen, float u, float v)
{
    float x = pen->mirror ? ICON_GRID - u : u;
    float y = v;
    if (pen->angle != 0.0f) {
        float centre = ICON_GRID * 0.5f;
        float dx = x - centre;
        float dy = y - centre;
        float c = cosf(pen->angle);
        float s = sinf(pen->angle);
        x = centre + dx * c - dy * s;
        y = centre + dx * s + dy * c;
    }
    return (Point){pen->originX + x * pen->unit, pen->originY + y * pen->unit};
}

/**
 * @brief Strokes a straight segment.
 * @param pen Drawing context.
 * @param u1 Start x in grid units.
 * @param v1 Start y in grid units.
 * @param u2 End x in grid units.
 * @param v2 End y in grid units.
 */
static void line(const Pen *pen, float u1, float v1, float u2, float v2)
{
    mask_add_capsule(pen->mask, at(pen, u1, v1), at(pen, u2, v2), pen->half, pen->half);
}

/**
 * @brief Strokes a polyline given as interleaved coordinate pairs.
 * @param pen Drawing context.
 * @param coordinates Interleaved x and y values in grid units.
 * @param count Number of vertices, at most ICON_MAX_VERTICES.
 * @param closed true to close the shape.
 */
static void poly(const Pen *pen, const float *coordinates, int count, bool closed)
{
    Point points[ICON_MAX_VERTICES];
    if (count > ICON_MAX_VERTICES) {
        count = ICON_MAX_VERTICES;
    }
    for (int i = 0; i < count; i++) {
        points[i] = at(pen, coordinates[i * 2], coordinates[i * 2 + 1]);
    }
    ui_mask_polyline(pen->mask, points, count, closed, pen->half);
}

/**
 * @brief Fills a polygon given as interleaved coordinate pairs.
 * @param pen Drawing context.
 * @param coordinates Interleaved x and y values in grid units.
 * @param count Number of vertices, at most ICON_MAX_VERTICES.
 */
static void poly_fill(const Pen *pen, const float *coordinates, int count)
{
    Point points[ICON_MAX_VERTICES];
    if (count > ICON_MAX_VERTICES) {
        count = ICON_MAX_VERTICES;
    }
    for (int i = 0; i < count; i++) {
        points[i] = at(pen, coordinates[i * 2], coordinates[i * 2 + 1]);
    }
    mask_add_polygon_fill(pen->mask, points, count);
}

/**
 * @brief Strokes a circular arc.
 * @param pen Drawing context.
 * @param u Center x in grid units.
 * @param v Center y in grid units.
 * @param radius Arc radius in grid units.
 * @param start Start angle in degrees.
 * @param end End angle in degrees.
 */
static void arc(const Pen *pen, float u, float v, float radius, float start, float end)
{
    if (pen->mirror) {
        start = 180.0f - start;
        end = 180.0f - end;
    }
    ui_mask_arc(pen->mask, at(pen, u, v), radius * pen->unit, start, end, pen->half);
}

/**
 * @brief Fills a dot.
 * @param pen Drawing context.
 * @param u Center x in grid units.
 * @param v Center y in grid units.
 * @param radius Radius in grid units.
 */
static void dot(const Pen *pen, float u, float v, float radius)
{
    mask_add_ellipse_fill(pen->mask, at(pen, u, v), radius * pen->unit, radius * pen->unit);
}

/**
 * @brief Strokes a rounded rectangle outline.
 * @param pen Drawing context.
 * @param u Left in grid units.
 * @param v Top in grid units.
 * @param width Width in grid units.
 * @param height Height in grid units.
 * @param radius Corner radius in grid units.
 */
static void box(const Pen *pen, float u, float v, float width, float height, float radius)
{
    Point corner = at(pen, u, v);
    mask_add_rect_stroke(pen->mask, corner.x, corner.y, width * pen->unit, height * pen->unit,
                         radius * pen->unit, pen->half * 2.0f);
}

/**
 * @brief Strokes a horizontal bar of arbitrary thickness.
 * @param pen Drawing context.
 * @param u1 Start x in grid units.
 * @param u2 End x in grid units.
 * @param v Center y in grid units.
 * @param thickness Thickness in grid units.
 */
static void bar(const Pen *pen, float u1, float u2, float v, float thickness)
{
    float radius = thickness * 0.5f * pen->unit;
    mask_add_capsule(pen->mask, at(pen, u1, v), at(pen, u2, v), radius, radius);
}

/**
 * @brief Draws a padlock; the open variant swings the shackle up.
 * @param pen Drawing context.
 * @param open true for the unlocked padlock.
 */
static void draw_lock(const Pen *pen, bool open)
{
    box(pen, 3.5f, 7.0f, 9.0f, 6.5f, 1.6f);
    dot(pen, 8.0f, 10.2f, 0.9f);
    if (open) {
        arc(pen, 8.0f, 4.4f, 3.0f, 180.0f, 360.0f);
        line(pen, 5.0f, 4.4f, 5.0f, 7.0f);
    } else {
        arc(pen, 8.0f, 5.0f, 3.0f, 180.0f, 360.0f);
        line(pen, 5.0f, 5.0f, 5.0f, 7.0f);
        line(pen, 11.0f, 5.0f, 11.0f, 7.0f);
    }
}

/**
 * @brief Draws the selection pointer.
 * @param pen Drawing context.
 */
static void draw_select(const Pen *pen)
{
    static const float shape[] = {4.0f,  2.5f,  4.0f,  12.6f, 6.7f, 10.2f, 8.7f,
                                  14.0f, 10.4f, 13.1f, 8.5f,  9.5f, 12.2f, 9.3f};
    poly(pen, shape, 7, true);
}

/**
 * @brief Draws the pen tool as a pencil.
 * @param pen Drawing context.
 */
static void draw_pencil(const Pen *pen)
{
    static const float shape[] = {2.8f, 13.2f, 3.8f, 9.8f, 10.2f, 3.4f, 12.6f, 5.8f, 6.2f, 12.2f};
    poly(pen, shape, 5, true);
    line(pen, 8.9f, 4.7f, 11.3f, 7.1f);
}

/**
 * @brief Draws a marker with the swipe it leaves.
 * @param pen Drawing context.
 */
static void draw_highlighter(const Pen *pen)
{
    static const float body[] = {5.2f, 9.6f, 10.4f, 4.4f, 12.8f, 6.8f, 7.6f, 12.0f};
    poly(pen, body, 4, true);
    line(pen, 2.4f, 14.0f, 9.0f, 14.0f);
}

/**
 * @brief Draws the laser pointer as a dot with rays.
 * @param pen Drawing context.
 */
static void draw_laser(const Pen *pen)
{
    dot(pen, 8.0f, 8.0f, 2.0f);
    for (int i = 0; i < 8; i++) {
        float angle = (float)i * PI_FLOAT / 4.0f;
        float c = cosf(angle);
        float s = sinf(angle);
        line(pen, 8.0f + c * 4.6f, 8.0f + s * 4.6f, 8.0f + c * 6.6f, 8.0f + s * 6.6f);
    }
}

/**
 * @brief Draws the eraser.
 * @param pen Drawing context.
 */
static void draw_eraser(const Pen *pen)
{
    static const float shape[] = {2.6f, 9.6f, 9.4f, 2.8f, 13.4f, 6.8f, 6.6f, 13.6f};
    poly(pen, shape, 4, true);
    line(pen, 6.0f, 6.2f, 10.0f, 10.2f);
    line(pen, 8.8f, 13.6f, 13.6f, 13.6f);
}

/**
 * @brief Draws the desktop or whiteboard mode symbol.
 * @param pen Drawing context.
 * @param whiteboard true for the whiteboard symbol.
 */
static void draw_mode(const Pen *pen, bool whiteboard)
{
    static const float scribble[] = {4.6f, 8.0f, 6.8f, 5.2f, 8.8f, 8.0f, 11.4f, 5.4f};
    if (whiteboard) {
        box(pen, 2.0f, 2.5f, 12.0f, 8.5f, 1.4f);
        poly(pen, scribble, 4, false);
        line(pen, 5.0f, 11.0f, 4.0f, 14.0f);
        line(pen, 11.0f, 11.0f, 12.0f, 14.0f);
    } else {
        box(pen, 2.0f, 3.0f, 12.0f, 8.0f, 1.4f);
        line(pen, 8.0f, 11.0f, 8.0f, 13.4f);
        line(pen, 5.4f, 13.4f, 10.6f, 13.4f);
    }
}

/**
 * @brief Draws the drawing on or off symbol: a pencil over a wavy line while drawing, a mouse
 *        arrow while the desktop stays interactive.
 * @param pen Drawing context.
 * @param on true when drawing is enabled.
 */
static void draw_drawing(const Pen *pen, bool on)
{
    static const float wave[] = {7.0f, 13.6f, 8.6f, 12.2f, 10.2f, 13.6f, 11.8f, 12.2f, 13.4f, 13.6f};
    static const float cursor[] = {4.0f,  2.4f,  4.0f,  12.8f, 6.6f, 10.4f, 8.4f,
                                   14.0f, 10.2f, 13.2f, 8.5f,  9.8f, 12.0f, 9.6f};
    if (on) {
        draw_pencil(pen);
        poly(pen, wave, 5, false);
    } else {
        poly(pen, cursor, 7, true);
        poly_fill(pen, cursor, 7);
    }
}

/**
 * @brief Draws two overlapping sheets, the copy symbol.
 * @param pen Drawing context.
 */
static void draw_copy(const Pen *pen)
{
    static const float back[] = {6.0f, 11.4f, 3.6f,  11.4f, 2.6f,  10.4f, 2.6f,  4.0f,
                                 3.6f, 3.0f,  11.0f, 3.0f,  12.0f, 4.0f,  12.0f, 6.0f};
    poly(pen, back, 8, false);
    box(pen, 6.0f, 6.0f, 8.0f, 8.0f, 1.4f);
}

/**
 * @brief Draws the font choice symbols: a slanted wobbly A, a plain A and a wide A with serifs.
 * @param pen Drawing context.
 * @param face 0 handwritten, 1 normal, 2 legible.
 */
static void draw_font(const Pen *pen, int face)
{
    if (face == 0) {
        static const float hand[] = {2.6f, 13.2f, 4.4f,  8.0f, 6.6f,  3.0f,
                                     8.6f, 5.4f,  10.6f, 9.6f, 13.4f, 13.4f};
        poly(pen, hand, 6, false);
        line(pen, 4.8f, 10.4f, 11.0f, 9.8f);
    } else if (face == 1) {
        static const float plain[] = {3.4f, 13.2f, 8.0f, 2.8f, 12.6f, 13.2f};
        poly(pen, plain, 3, false);
        line(pen, 5.2f, 9.6f, 10.8f, 9.6f);
    } else {
        static const float wide[] = {3.0f, 13.0f, 8.0f, 2.8f, 13.0f, 13.0f};
        poly(pen, wide, 3, false);
        line(pen, 4.8f, 9.4f, 11.2f, 9.4f);
        line(pen, 1.8f, 13.0f, 4.6f, 13.0f);
        line(pen, 11.4f, 13.0f, 14.2f, 13.0f);
        line(pen, 6.6f, 2.8f, 9.4f, 2.8f);
    }
}

/**
 * @brief Draws a floppy disk, the save symbol.
 * @param pen Drawing context.
 */
static void draw_save(const Pen *pen)
{
    static const float shutter[] = {5.2f, 2.5f, 5.2f, 6.2f, 10.8f, 6.2f, 10.8f, 2.5f};
    static const float label[] = {5.2f, 13.5f, 5.2f, 9.6f, 10.8f, 9.6f, 10.8f, 13.5f};
    box(pen, 2.5f, 2.5f, 11.0f, 11.0f, 1.4f);
    poly(pen, shutter, 4, false);
    poly(pen, label, 4, false);
}

/**
 * @brief Draws the power symbol used for emergency exit.
 * @param pen Drawing context.
 */
static void draw_exit(const Pen *pen)
{
    arc(pen, 8.0f, 8.6f, 5.0f, -50.0f, 230.0f);
    line(pen, 8.0f, 2.2f, 8.0f, 7.6f);
}

/**
 * @brief Draws the undo arrow; the pen mirrors it for redo.
 * @param pen Drawing context.
 */
static void draw_undo(const Pen *pen)
{
    line(pen, 3.0f, 6.5f, 5.9f, 3.8f);
    line(pen, 3.0f, 6.5f, 5.9f, 9.2f);
    line(pen, 3.0f, 6.5f, 8.5f, 6.5f);
    arc(pen, 8.5f, 10.0f, 3.5f, -90.0f, 90.0f);
    line(pen, 8.5f, 13.5f, 5.8f, 13.5f);
}

/**
 * @brief Draws a stroke style sample.
 * @param pen Drawing context.
 * @param style 0 solid, 1 dashed, 2 dotted.
 */
static void draw_stroke_style(const Pen *pen, int style)
{
    if (style == 0) {
        bar(pen, 2.4f, 13.6f, 8.0f, 1.6f);
    } else if (style == 1) {
        bar(pen, 2.2f, 4.8f, 8.0f, 1.6f);
        bar(pen, 6.6f, 9.4f, 8.0f, 1.6f);
        bar(pen, 11.2f, 13.8f, 8.0f, 1.6f);
    } else {
        for (int i = 0; i < 4; i++) {
            dot(pen, 2.8f + (float)i * 3.5f, 8.0f, 0.95f);
        }
    }
}

/**
 * @brief Draws the grip of six dots.
 * @param pen Drawing context.
 */
static void draw_grip(const Pen *pen)
{
    for (int column = 0; column < 2; column++) {
        for (int row = 0; row < 3; row++) {
            dot(pen, 5.8f + (float)column * 4.4f, 3.8f + (float)row * 4.2f, 1.05f);
        }
    }
}

/**
 * @brief Draws the letter A of the text tool.
 * @param pen Drawing context.
 */
static void draw_text(const Pen *pen)
{
    static const float letter[] = {3.4f, 13.2f, 8.0f, 2.8f, 12.6f, 13.2f};
    poly(pen, letter, 3, false);
    line(pen, 5.2f, 9.6f, 10.8f, 9.6f);
}

/**
 * @brief Draws the push pin; pinned is upright and solid, unpinned is an outline tilted by 45 degrees.
 * @param pen Drawing context; its rotation is set here for the unpinned variant.
 * @param pinned true for the pinned look.
 */
static void draw_pin(Pen *pen, bool pinned)
{
    static const float body[] = {5.8f,  2.2f,  10.2f, 2.2f,  10.2f, 6.2f, 12.2f, 8.6f,
                                 12.2f, 10.0f, 3.8f,  10.0f, 3.8f,  8.6f, 5.8f,  6.2f};
    if (!pinned) {
        pen->angle = PI_FLOAT * 0.25f;
    }
    poly(pen, body, 8, true);
    if (pinned) {
        poly_fill(pen, body, 8);
    }
    line(pen, 8.0f, 10.0f, 8.0f, 14.2f);
}

/**
 * @brief Draws a settings gear: a ring with eight teeth and a hub.
 * @param pen Drawing context.
 */
static void draw_gear(const Pen *pen)
{
    mask_add_ellipse_stroke(pen->mask, at(pen, 8.0f, 8.0f), 4.4f * pen->unit, 4.4f * pen->unit,
                            pen->half * 2.0f);
    mask_add_ellipse_stroke(pen->mask, at(pen, 8.0f, 8.0f), 1.7f * pen->unit, 1.7f * pen->unit,
                            pen->half * 2.0f);
    for (int tooth = 0; tooth < 8; tooth++) {
        float angle = (float)tooth * PI_FLOAT / 4.0f;
        float c = cosf(angle);
        float s = sinf(angle);
        line(pen, 8.0f + 5.2f * c, 8.0f + 5.2f * s, 8.0f + 6.9f * c, 8.0f + 6.9f * s);
    }
}

/**
 * @brief Draws a trash can.
 * @param pen Drawing context.
 */
static void draw_trash(const Pen *pen)
{
    static const float lid[4] = {2.8f, 4.4f, 13.2f, 4.4f};
    static const float handle[8] = {6.2f, 4.4f, 6.2f, 2.6f, 9.8f, 2.6f, 9.8f, 4.4f};
    static const float body[8] = {4.2f, 4.4f, 5.0f, 13.6f, 11.0f, 13.6f, 11.8f, 4.4f};
    line(pen, lid[0], lid[1], lid[2], lid[3]);
    poly(pen, handle, 4, false);
    poly(pen, body, 4, false);
    line(pen, 6.7f, 7.0f, 6.9f, 11.6f);
    line(pen, 9.3f, 7.0f, 9.1f, 11.6f);
}

/**
 * @brief Draws an eye; the hidden variant has a slash across it.
 * @param pen Drawing context.
 * @param hidden true for the crossed-out eye.
 */
static void draw_eye(const Pen *pen, bool hidden)
{
    static const float outline[16] = {1.4f,  8.0f, 3.8f,  5.2f,  8.0f, 3.8f,  12.2f, 5.2f,
                                      14.6f, 8.0f, 12.2f, 10.8f, 8.0f, 12.2f, 3.8f,  10.8f};
    poly(pen, outline, 8, true);
    dot(pen, 8.0f, 8.0f, 2.0f);
    if (hidden) {
        line(pen, 3.0f, 13.4f, 13.0f, 2.6f);
    }
}

/**
 * @brief Dispatches to the drawing routine of an icon.
 * @param pen Drawing context.
 * @param icon Icon to draw.
 * @return false when the icon is unknown.
 */
static bool draw_icon(Pen *pen, Icon icon)
{
    switch (icon) {
    case ICON_GRIP:
        draw_grip(pen);
        break;
    case ICON_LOCK_CLOSED:
        draw_lock(pen, false);
        break;
    case ICON_LOCK_OPEN:
        draw_lock(pen, true);
        break;
    case ICON_SELECT:
        draw_select(pen);
        break;
    case ICON_RECTANGLE:
        box(pen, 2.5f, 3.5f, 11.0f, 9.0f, 1.6f);
        break;
    case ICON_ELLIPSE:
        mask_add_ellipse_stroke(pen->mask, at(pen, 8.0f, 8.0f), 5.6f * pen->unit, 5.6f * pen->unit,
                                pen->half * 2.0f);
        break;
    case ICON_ARROW:
        line(pen, 3.0f, 13.0f, 13.0f, 3.0f);
        line(pen, 13.0f, 3.0f, 7.4f, 3.0f);
        line(pen, 13.0f, 3.0f, 13.0f, 8.6f);
        break;
    case ICON_LINE:
        line(pen, 4.0f, 12.0f, 12.0f, 4.0f);
        dot(pen, 3.4f, 12.6f, 1.5f);
        dot(pen, 12.6f, 3.4f, 1.5f);
        break;
    case ICON_PEN:
        draw_pencil(pen);
        break;
    case ICON_HIGHLIGHTER:
        draw_highlighter(pen);
        break;
    case ICON_LASER:
        draw_laser(pen);
        break;
    case ICON_TEXT:
        draw_text(pen);
        break;
    case ICON_ERASER:
        draw_eraser(pen);
        break;
    case ICON_MODE_DESKTOP:
        draw_mode(pen, false);
        break;
    case ICON_MODE_WHITEBOARD:
        draw_mode(pen, true);
        break;
    case ICON_DRAWING_ON:
        draw_drawing(pen, true);
        break;
    case ICON_DRAWING_OFF:
        draw_drawing(pen, false);
        break;
    case ICON_EXIT:
        draw_exit(pen);
        break;
    case ICON_UNDO:
        draw_undo(pen);
        break;
    case ICON_REDO:
        pen->mirror = true;
        draw_undo(pen);
        break;
    case ICON_COPY:
        draw_copy(pen);
        break;
    case ICON_SAVE:
        draw_save(pen);
        break;
    case ICON_LASER_TRAIL: {
        static const float trail[] = {2.4f, 12.6f, 5.0f, 8.0f, 8.0f, 9.6f, 10.6f, 5.4f};
        poly(pen, trail, 4, false);
        dot(pen, 12.2f, 4.2f, 1.8f);
        break;
    }
    case ICON_LASER_POINTER:
        dot(pen, 8.0f, 8.0f, 2.4f);
        arc(pen, 8.0f, 8.0f, 5.6f, 0.0f, 360.0f);
        break;
    case ICON_FONT_HAND:
        draw_font(pen, 0);
        break;
    case ICON_FONT_NORMAL:
        draw_font(pen, 1);
        break;
    case ICON_FONT_LEGIBLE:
        draw_font(pen, 2);
        break;
    case ICON_STYLE_SOLID:
        draw_stroke_style(pen, 0);
        break;
    case ICON_STYLE_DASHED:
        draw_stroke_style(pen, 1);
        break;
    case ICON_STYLE_DOTTED:
        draw_stroke_style(pen, 2);
        break;
    case ICON_WIDTH_THIN:
        bar(pen, 2.6f, 13.4f, 8.0f, 1.2f);
        break;
    case ICON_WIDTH_MEDIUM:
        bar(pen, 2.6f, 13.4f, 8.0f, 2.6f);
        break;
    case ICON_WIDTH_BOLD:
        bar(pen, 2.6f, 13.4f, 8.0f, 4.4f);
        break;
    case ICON_PRESSURE:
        mask_add_capsule(pen->mask, at(pen, 2.6f, 8.0f), at(pen, 13.4f, 8.0f), 0.5f * pen->unit,
                         2.6f * pen->unit);
        break;
    case ICON_SETTINGS:
        draw_gear(pen);
        break;
    case ICON_TRASH:
        draw_trash(pen);
        break;
    case ICON_EYE:
        draw_eye(pen, false);
        break;
    case ICON_EYE_OFF:
        draw_eye(pen, true);
        break;
    case ICON_CLOSE:
        line(pen, 4.0f, 4.0f, 12.0f, 12.0f);
        line(pen, 12.0f, 4.0f, 4.0f, 12.0f);
        break;
    case ICON_CHEVRON_UP: {
        static const float shape[6] = {4.0f, 10.0f, 8.0f, 6.0f, 12.0f, 10.0f};
        poly(pen, shape, 3, false);
        break;
    }
    case ICON_CHEVRON_DOWN: {
        static const float shape[6] = {4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 6.0f};
        poly(pen, shape, 3, false);
        break;
    }
    case ICON_PIN:
        draw_pin(pen, true);
        break;
    case ICON_PIN_OFF:
        draw_pin(pen, false);
        break;
    case ICON_DIAMOND: {
        static const float shape[8] = {8.0f, 2.2f, 13.8f, 8.0f, 8.0f, 13.8f, 2.2f, 8.0f};
        poly(pen, shape, 4, true);
        break;
    }
    case ICON_ARROW_END:
    case ICON_ARROW_BOTH: {
        line(pen, 2.4f, 8.0f, 13.6f, 8.0f);
        line(pen, 13.6f, 8.0f, 9.6f, 4.6f);
        line(pen, 13.6f, 8.0f, 9.6f, 11.4f);
        if (icon == ICON_ARROW_BOTH) {
            line(pen, 2.4f, 8.0f, 6.4f, 4.6f);
            line(pen, 2.4f, 8.0f, 6.4f, 11.4f);
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

void icon_draw(Canvas *canvas, Icon icon, float centerX, float centerY, float size, Color color)
{
    float unit = size / ICON_GRID;
    float originX = centerX - size * 0.5f;
    float originY = centerY - size * 0.5f;
    int pad = 3;
    int extent = (int)ceilf(size) + pad * 2 + 1;
    Mask *mask = ui_scratch_mask((int)floorf(originX) - pad, (int)floorf(originY) - pad, extent, extent);
    if (!mask) {
        return;
    }
    Pen pen = {mask, originX, originY, unit, ICON_HALF_STROKE * unit, false, 0.0f};
    if (draw_icon(&pen, icon)) {
        mask_blend(canvas, mask, color);
    }
}

Icon icon_for_tool(int tool)
{
    static const Icon icons[TOOL_COUNT] = {ICON_SELECT, ICON_RECTANGLE, ICON_ELLIPSE,    ICON_ARROW,
                                           ICON_LINE,   ICON_PEN,       ICON_LASER,      ICON_TEXT,
                                           ICON_ERASER, ICON_DIAMOND,   ICON_HIGHLIGHTER};
    return (unsigned)tool < TOOL_COUNT ? icons[tool] : ICON_NONE;
}
