/**
 * @file icons.h
 * @brief Stroke-based vector icons drawn with the rasterizer on a 16 unit grid.
 */
#ifndef INK_ICONS_H
#define INK_ICONS_H

#include "canvas.h"

/** @brief Every icon the UI draws. */
typedef enum Icon {
    ICON_NONE,
    ICON_GRIP,
    ICON_LOCK_CLOSED,
    ICON_LOCK_OPEN,
    ICON_SELECT,
    ICON_RECTANGLE,
    ICON_ELLIPSE,
    ICON_ARROW,
    ICON_LINE,
    ICON_PEN,
    ICON_LASER,
    ICON_TEXT,
    ICON_ERASER,
    ICON_MODE_DESKTOP,
    ICON_MODE_WHITEBOARD,
    ICON_DRAWING_ON,
    ICON_DRAWING_OFF,
    ICON_EXIT,
    ICON_UNDO,
    ICON_REDO,
    ICON_STYLE_SOLID,
    ICON_STYLE_DASHED,
    ICON_STYLE_DOTTED,
    ICON_WIDTH_THIN,
    ICON_WIDTH_MEDIUM,
    ICON_WIDTH_BOLD,
    ICON_COUNT
} Icon;

/**
 * @brief Draws an icon centered at a point.
 * @param canvas Target canvas.
 * @param icon Icon to draw.
 * @param centerX Center x in canvas pixels.
 * @param centerY Center y in canvas pixels.
 * @param size Edge length of the 16 unit icon box in pixels.
 * @param color Straight-alpha color.
 */
void icon_draw(Canvas *canvas, Icon icon, float centerX, float centerY, float size, Color color);

/**
 * @brief Returns the icon that represents a drawing tool.
 * @param tool Value of the Tool enum.
 * @return Matching icon, ICON_NONE when out of range.
 */
Icon icon_for_tool(int tool);

#endif
