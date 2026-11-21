/**
 * @file theme.h
 * @brief Visual constants taken from Excalidraw's dark theme.
 *
 * Colors are straight (non-premultiplied) 0xAARRGGBB values. Sizes are in pixels at 100% scaling.
 * See docs/excalidraw-reference.md for the source of every value.
 */
#ifndef INK_THEME_H
#define INK_THEME_H

#include <stdint.h>

/** @brief Straight-alpha color packed as 0xAARRGGBB. */
typedef uint32_t Color;

#define COLOR_CANVAS_DARK 0xFF121212u
#define COLOR_ISLAND 0xFF232329u
#define COLOR_ISLAND_HOVER 0xFF2E2D39u
#define COLOR_ISLAND_SELECTED 0xFF403E6Au
#define COLOR_ICON 0xFFE3E3E8u
#define COLOR_ICON_SELECTED 0xFFE0DFFFu
#define COLOR_ACTIVE_BORDER 0xFFD0CCFFu
#define COLOR_KEYBINDING 0xFF7A7A7Au
#define COLOR_PRIMARY 0xFFA8A5FFu

#define COLOR_STROKE_DEFAULT 0xFFD3D3D3u
#define COLOR_STROKE_RED 0xFFFF8383u
#define COLOR_STROKE_GREEN 0xFF3A994Cu
#define COLOR_STROKE_BLUE 0xFF56A2E8u
#define COLOR_STROKE_ORANGE 0xFFB76100u

#define COLOR_FILL_RED 0xFF5B2D2Du
#define COLOR_FILL_GREEN 0xFF043B0Cu
#define COLOR_FILL_BLUE 0xFF154163u
#define COLOR_FILL_YELLOW 0xFF362600u

#define COLOR_LASER 0xFFFF0000u

#define ISLAND_RADIUS 8.0f
#define BUTTON_SIZE 36
#define ICON_SIZE 16
#define ISLAND_PADDING 4

#define STROKE_WIDTH_THIN 1.0f
#define STROKE_WIDTH_MEDIUM 2.0f
#define STROKE_WIDTH_BOLD 4.0f

#define EMERGENCY_HOTKEY_ID 1

#endif
