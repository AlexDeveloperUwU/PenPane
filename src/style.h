/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file style.h
 * @brief Element appearance properties and the Excalidraw dark palettes.
 */
#ifndef PENPANE_STYLE_H
#define PENPANE_STYLE_H

#include <stdbool.h>

#include "theme.h"

#define STROKE_PALETTE_SIZE 5
#define FILL_PALETTE_SIZE 5
#define WIDTH_PRESET_COUNT 3
#define FONT_SIZE_COUNT 4
#define PASTEL_PALETTE_SIZE 6
#define CUSTOM_COLOR_MAX 18

/** @brief Line pattern of strokes. */
typedef enum StrokeStyle { STROKE_SOLID, STROKE_DASHED, STROKE_DOTTED } StrokeStyle;

/** @brief Whether a line has no head, a head at its end only or heads at both ends. */
typedef enum ArrowHeads { ARROW_HEAD_NONE, ARROW_HEAD_END, ARROW_HEAD_BOTH, ARROW_HEAD_COUNT } ArrowHeads;

/** @brief Corner shape of rectangles. */
typedef enum Corners { CORNERS_ROUND, CORNERS_SHARP, CORNERS_COUNT } Corners;

/** @brief How the background of a filled shape is drawn. */
typedef enum FillStyle {
    FILL_STYLE_SOLID,
    FILL_STYLE_HACHURE,
    FILL_STYLE_CROSS_HATCH,
    FILL_STYLE_COUNT
} FillStyle;

/** @brief Horizontal alignment of the lines of a text element. */
typedef enum TextAlign { TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT, TEXT_ALIGN_COUNT } TextAlign;

/** @brief Typeface of text elements. */
typedef enum TextFace {
    TEXT_FACE_HANDWRITTEN,
    TEXT_FACE_NORMAL,
    TEXT_FACE_LEGIBLE,
    TEXT_FACE_COUNT
} TextFace;

/** @brief Appearance applied to new elements and editable on selected ones. */
typedef struct Style {
    Color stroke;
    Color fill;
    float width;
    float opacity;
    StrokeStyle strokeStyle;
    bool pressure;
    TextFace face;
    ArrowHeads arrowHeads;
    bool highlighter;
    int fontSize;
    Corners corners;
    FillStyle fillStyle;
    TextAlign align;
} Style;

/** @brief The five stroke preset colors in dark theme. */
extern const Color STROKE_PALETTE[STROKE_PALETTE_SIZE];

/** @brief The optional pastel stroke row. */
extern const Color PASTEL_PALETTE[PASTEL_PALETTE_SIZE];

/** @brief The five background presets; entry 0 is fully transparent. */
extern const Color FILL_PALETTE[FILL_PALETTE_SIZE];

/** @brief Stroke widths: thin, medium, bold. */
extern const float WIDTH_PRESETS[WIDTH_PRESET_COUNT];

/** @brief Highlighter widths: thin, medium, bold. Wider than WIDTH_PRESETS and always drawn translucent. */
extern const float HIGHLIGHTER_WIDTHS[WIDTH_PRESET_COUNT];

/** @brief Text heights in pixels: S, M, L, XL. */
extern const int FONT_SIZES[FONT_SIZE_COUNT];

/**
 * @brief Finds the preset index of a style's text size.
 * @param style Style to look up.
 * @return Index into FONT_SIZES of the nearest size.
 */
int style_font_size_index(const Style *style);

/**
 * @brief Returns the Excalidraw default style: first stroke preset, transparent fill, medium width.
 * @return Default style value.
 */
Style style_default(void);

/**
 * @brief Returns the width of a preset button.
 * @param style Style deciding between the pen and the highlighter table.
 * @param index Preset index, 0 to WIDTH_PRESET_COUNT - 1.
 * @return Width in pixels.
 */
float style_width_preset(const Style *style, int index);

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
 * @brief Finds the preset index of a style's stroke width.
 * @param style Style whose width is looked up in the pen or highlighter table.
 * @return Index of the nearest preset.
 */
int style_width_index(const Style *style);

/**
 * @brief Converts hue, saturation and value to an opaque color.
 * @param hue 0 to 1, wrapping.
 * @param saturation 0 to 1.
 * @param value 0 to 1.
 * @return Opaque straight-alpha color.
 */
Color color_from_hsv(float hue, float saturation, float value);

/**
 * @brief Converts a color to hue, saturation and value; alpha is ignored.
 * @param color Color to convert.
 * @param hue Receives 0 to 1 (0 for grays).
 * @param saturation Receives 0 to 1.
 * @param value Receives 0 to 1.
 */
void color_to_hsv(Color color, float *hue, float *saturation, float *value);

/**
 * @brief Parses "RRGGBB" or "#RRGGBB".
 * @param text Zero-terminated text.
 * @param color Receives the opaque color on success.
 * @return true when the text is a valid hex color.
 */
bool color_parse_hex(const char *text, Color *color);

#endif
