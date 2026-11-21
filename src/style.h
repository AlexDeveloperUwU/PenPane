/**
 * @file style.h
 * @brief Element appearance properties and the Excalidraw dark palettes.
 */
#ifndef INK_STYLE_H
#define INK_STYLE_H

#include <stdbool.h>

#include "theme.h"

#define STROKE_PALETTE_SIZE 5
#define FILL_PALETTE_SIZE 5
#define WIDTH_PRESET_COUNT 3

/** @brief Line pattern of strokes. */
typedef enum StrokeStyle {
    STROKE_SOLID,
    STROKE_DASHED,
    STROKE_DOTTED
} StrokeStyle;

/** @brief Appearance applied to new elements and editable on selected ones. */
typedef struct Style {
    Color stroke;
    Color fill;
    float width;
    float opacity;
    StrokeStyle strokeStyle;
    bool pressure;
} Style;

/** @brief The five stroke preset colors in dark theme. */
extern const Color STROKE_PALETTE[STROKE_PALETTE_SIZE];

/** @brief The five background presets; entry 0 is fully transparent. */
extern const Color FILL_PALETTE[FILL_PALETTE_SIZE];

/** @brief Stroke widths: thin, medium, bold. */
extern const float WIDTH_PRESETS[WIDTH_PRESET_COUNT];

/**
 * @brief Returns the Excalidraw default style: first stroke preset, transparent fill, medium width.
 * @return Default style value.
 */
Style style_default(void);

/**
 * @brief Finds the preset index of a stroke color.
 * @param color Straight-alpha color.
 * @return Index into STROKE_PALETTE, or -1 when the color is not a preset.
 */
int style_stroke_index(Color color);

/**
 * @brief Finds the preset index of a background color.
 * @param color Straight-alpha color, 0 for transparent.
 * @return Index into FILL_PALETTE, or -1 when the color is not a preset.
 */
int style_fill_index(Color color);

/**
 * @brief Finds the preset index of a stroke width.
 * @param width Width in pixels.
 * @return Index into WIDTH_PRESETS of the nearest preset.
 */
int style_width_index(float width);

#endif
