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
} Pen;

/**
 * @brief Maps grid coordinates to mask coordinates, mirroring horizontally when requested.
 * @param pen Drawing context.
 * @param u Grid x.
 * @param v Grid y.
 * @return Point in canvas pixels.
 */
static Point at(const Pen *pen, float u, float v)
{
    float x = pen->mirror ? ICON_GRID - u : u;
    return (Point){pen->originX + x * pen->unit, pen->originY + v * pen->unit};
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
    static const float shape[] = {4.0f, 2.5f, 4.0f, 12.6f, 6.7f, 10.2f, 8.7f, 14.0f,
                                  10.4f, 13.1f, 8.5f, 9.5f, 12.2f, 9.3f};
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
 * @brief Draws the drawing on or off symbol, a freehand scribble that is struck out when off.
 * @param pen Drawing context.
 * @param on true when drawing is enabled.
 */
static void draw_drawing(const Pen *pen, bool on)
{
    static const float scribble[] = {2.2f, 10.8f, 4.6f, 5.4f,  7.2f,  10.6f,
                                     9.6f, 5.4f,  11.4f, 8.4f, 13.8f, 6.6f};
    poly(pen, scribble, 6, false);
    if (!on) {
        line(pen, 2.6f, 13.6f, 13.4f, 2.4f);
    }
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
 * @brief Dispatches to the drawing routine of an icon.
 * @param pen Drawing context.
 * @param icon Icon to draw.
 * @return false when the icon is unknown.
 */
static bool draw_icon(Pen *pen, Icon icon)
{
    switch (icon) {
    case ICON_GRIP: draw_grip(pen); break;
    case ICON_LOCK_CLOSED: draw_lock(pen, false); break;
    case ICON_LOCK_OPEN: draw_lock(pen, true); break;
    case ICON_SELECT: draw_select(pen); break;
    case ICON_RECTANGLE: box(pen, 2.5f, 3.5f, 11.0f, 9.0f, 1.6f); break;
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
    case ICON_PEN: draw_pencil(pen); break;
    case ICON_LASER: draw_laser(pen); break;
    case ICON_TEXT: draw_text(pen); break;
    case ICON_ERASER: draw_eraser(pen); break;
    case ICON_MODE_DESKTOP: draw_mode(pen, false); break;
    case ICON_MODE_WHITEBOARD: draw_mode(pen, true); break;
    case ICON_DRAWING_ON: draw_drawing(pen, true); break;
    case ICON_DRAWING_OFF: draw_drawing(pen, false); break;
    case ICON_EXIT: draw_exit(pen); break;
    case ICON_UNDO: draw_undo(pen); break;
    case ICON_REDO:
        pen->mirror = true;
        draw_undo(pen);
        break;
    case ICON_STYLE_SOLID: draw_stroke_style(pen, 0); break;
    case ICON_STYLE_DASHED: draw_stroke_style(pen, 1); break;
    case ICON_STYLE_DOTTED: draw_stroke_style(pen, 2); break;
    case ICON_WIDTH_THIN: bar(pen, 2.6f, 13.4f, 8.0f, 1.2f); break;
    case ICON_WIDTH_MEDIUM: bar(pen, 2.6f, 13.4f, 8.0f, 2.6f); break;
    case ICON_WIDTH_BOLD: bar(pen, 2.6f, 13.4f, 8.0f, 4.4f); break;
    default: return false;
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
    Pen pen = {mask, originX, originY, unit, ICON_HALF_STROKE * unit, false};
    if (draw_icon(&pen, icon)) {
        mask_blend(canvas, mask, color);
    }
}

Icon icon_for_tool(int tool)
{
    static const Icon icons[TOOL_COUNT] = {ICON_SELECT, ICON_RECTANGLE, ICON_ELLIPSE,
                                           ICON_ARROW,  ICON_LINE,      ICON_PEN,
                                           ICON_LASER,  ICON_TEXT,      ICON_ERASER};
    return (unsigned)tool < TOOL_COUNT ? icons[tool] : ICON_NONE;
}
