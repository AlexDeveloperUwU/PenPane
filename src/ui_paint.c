/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "shapes_ui.h"
#include "text.h"
#include "ui_internal.h"

#define COLOR_GRIP 0xFF6E6E7Au
#define COLOR_GRIP_HOVER 0xFF9A9AA6u
#define COLOR_DIVIDER 0xFF3F3F49u
#define COLOR_TITLE 0xFF9C9CA8u
#define COLOR_HEADER 0xFF2E2E37u
#define COLOR_PRESSED 0xFF383750u
#define COLOR_TRACK 0xFF3A3A44u
#define COLOR_TRACK_HOVER 0xFF474752u
#define COLOR_KNOB_OFF 0xFFC8C8D0u
#define COLOR_KNOB_ON 0xFF232329u
#define COLOR_SWATCH_BORDER 0x2EFFFFFFu
#define COLOR_SWATCH_HOVER 0x66FFFFFFu
#define COLOR_CHECKER_LIGHT 0xFF5C5C66u
#define COLOR_CHECKER_DARK 0xFF3A3A43u
#define COLOR_DISABLED_ALPHA 0x4D000000u

#define SWATCH_SIZE 20
#define SWATCH_RADIUS 4
#define SWATCH_RING_RADIUS 6
#define CHECKER_CELL 5
#define SLIDER_TRACK_HEIGHT 4
#define SLIDER_THUMB_RADIUS 7
#define TITLE_FONT_SIZE 12
#define HINT_FONT_SIZE 10
#define BUTTON_RADIUS 8
#define HANDLE_RADIUS 10
#define HANDLE_CHIP 12

/**
 * @brief Replaces the alpha channel of a color.
 * @param color Straight-alpha color.
 * @param alpha New alpha 0 to 255.
 * @return Color with the new alpha.
 */
static Color with_alpha(Color color, unsigned alpha)
{
    return (color & 0x00FFFFFFu) | (alpha << 24);
}

/**
 * @brief Scales the brightness of a color, keeping its alpha.
 * @param color Straight-alpha color.
 * @param factor Multiplier for the red, green and blue channels.
 * @return Adjusted color.
 */
static Color shade(Color color, float factor)
{
    Color result = color & 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8) {
        float channel = (float)((color >> shift) & 0xFFu) * factor;
        result |= (Color)(channel > 255.0f ? 255.0f : channel) << shift;
    }
    return result;
}

/**
 * @brief Draws one line of UI text inside a box.
 * @param canvas Target canvas.
 * @param text Text to draw.
 * @param size Pixel size of the font.
 * @param box Box the text is aligned in.
 * @param alignRight true to right-align, otherwise left-align.
 * @param color Text color.
 */
static void draw_label(Canvas *canvas, const wchar_t *text, int size, const RECT *box, bool alignRight,
                       Color color)
{
    int width;
    int height;
    text_measure(TEXT_FONT_UI, text, size, &width, &height);
    int x = alignRight ? box->right - width : box->left;
    int y = box->top + (box->bottom - box->top - height) / 2;
    Mask mask;
    if (text_make_mask(&mask, TEXT_FONT_UI, text, size, x, y)) {
        mask_blend(canvas, &mask, color);
        mask_destroy(&mask);
    }
}

/**
 * @brief Draws one line of UI text centered in a box.
 * @param canvas Target canvas.
 * @param text Text to draw.
 * @param size Pixel size of the font.
 * @param box Box the text is centered in.
 * @param color Text color.
 */
static void draw_label_centered(Canvas *canvas, const wchar_t *text, int size, const RECT *box, Color color)
{
    int width;
    int height;
    text_measure(TEXT_FONT_UI, text, size, &width, &height);
    int x = box->left + (box->right - box->left - width) / 2;
    int y = box->top + (box->bottom - box->top - height) / 2;
    Mask mask;
    if (text_make_mask(&mask, TEXT_FONT_UI, text, size, x, y)) {
        mask_blend(canvas, &mask, color);
        mask_destroy(&mask);
    }
}

/**
 * @brief Draws the checkerboard used for the transparent background swatch.
 * @param canvas Target canvas.
 * @param left Swatch left.
 * @param top Swatch top.
 * @param size Swatch edge.
 * @param radius Corner radius.
 * @param scale Scale factor.
 */
static void draw_checker(Canvas *canvas, float left, float top, float size, float radius, float scale)
{
    int cell = ui_px(scale, CHECKER_CELL) > 0 ? ui_px(scale, CHECKER_CELL) : 1;
    Mask *mask = ui_scratch_mask((int)floorf(left) - 1, (int)floorf(top) - 1, (int)ceilf(size) + 3,
                                 (int)ceilf(size) + 3);
    if (!mask) {
        return;
    }
    mask_add_rect_fill(mask, left, top, size, size, radius);
    mask_blend(canvas, mask, COLOR_CHECKER_LIGHT);
    for (int row = 0; row < mask->height; row++) {
        for (int column = 0; column < mask->width; column++) {
            int cellX = (mask->x + column - (int)floorf(left)) / cell;
            int cellY = (mask->y + row - (int)floorf(top)) / cell;
            if (((cellX + cellY) & 1) == 0 || mask->x + column < (int)floorf(left) ||
                mask->y + row < (int)floorf(top)) {
                mask->coverage[(size_t)row * mask->width + column] = 0;
            }
        }
    }
    mask_blend(canvas, mask, COLOR_CHECKER_DARK);
}

/**
 * @brief Draws a color swatch with a subtle border.
 * @param canvas Target canvas.
 * @param cell Rectangle of the swatch cell in canvas pixels.
 * @param color Swatch color.
 * @param checker true to draw the transparent checkerboard instead of a color.
 * @param scale Scale factor.
 */
static void draw_swatch(Canvas *canvas, const RECT *cell, Color color, bool checker, float scale)
{
    float size = (float)ui_px(scale, SWATCH_SIZE);
    float left = (float)cell->left + ((float)(cell->right - cell->left) - size) * 0.5f;
    float top = (float)cell->top + ((float)(cell->bottom - cell->top) - size) * 0.5f;
    float radius = (float)ui_px(scale, SWATCH_RADIUS);
    if (checker) {
        draw_checker(canvas, left, top, size, radius, scale);
    } else {
        ui_fill_round_rect(canvas, left, top, size, size, radius, color);
    }
    ui_stroke_round_rect(canvas, left + 0.5f, top + 0.5f, size - 1.0f, size - 1.0f, radius - 0.5f, 1.0f,
                         COLOR_SWATCH_BORDER);
}

/**
 * @brief Draws a selection or hover ring around a swatch cell.
 * @param canvas Target canvas.
 * @param cell Rectangle of the swatch cell in canvas pixels.
 * @param thickness Ring thickness in pixels.
 * @param color Ring color.
 * @param scale Scale factor.
 */
static void draw_ring(Canvas *canvas, const RECT *cell, float thickness, Color color, float scale)
{
    float inset = thickness * 0.5f;
    ui_stroke_round_rect(canvas, (float)cell->left + inset, (float)cell->top + inset,
                         (float)(cell->right - cell->left) - thickness,
                         (float)(cell->bottom - cell->top) - thickness,
                         (float)ui_px(scale, SWATCH_RING_RADIUS), thickness, color);
}

/**
 * @brief Draws the keybinding hint in the bottom-right corner of a button.
 * @param canvas Target canvas.
 * @param rect Button rectangle in canvas pixels.
 * @param hint Hint character.
 * @param scale Scale factor.
 */
static void draw_hint(Canvas *canvas, const RECT *rect, wchar_t hint, float scale)
{
    wchar_t text[2] = {hint, 0};
    int width;
    int height;
    int size = ui_px(scale, HINT_FONT_SIZE);
    text_measure(TEXT_FONT_UI, text, size, &width, &height);
    Mask mask;
    int x = rect->right - ui_px(scale, 4) - width;
    int y = rect->bottom - ui_px(scale, 2) - height;
    if (text_make_mask(&mask, TEXT_FONT_UI, text, size, x, y)) {
        mask_blend(canvas, &mask, COLOR_KEYBINDING);
        mask_destroy(&mask);
    }
}

/**
 * @brief Paints an icon button.
 * @param canvas Target canvas.
 * @param item Button item.
 * @param rect Button rectangle in canvas pixels.
 * @param state Interaction state.
 * @param scale Scale factor.
 */
static void paint_button(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state, float scale)
{
    Color background = 0;
    if (item->selected) {
        background = COLOR_ISLAND_SELECTED;
    } else if (item->enabled && state.pressed) {
        background = COLOR_PRESSED;
    } else if (item->enabled && state.hover) {
        background = COLOR_ISLAND_HOVER;
    }
    if (background) {
        ui_fill_round_rect(canvas, (float)rect->left, (float)rect->top, (float)(rect->right - rect->left),
                           (float)(rect->bottom - rect->top), (float)ui_px(scale, BUTTON_RADIUS), background);
    }
    Color color = item->selected ? COLOR_ICON_SELECTED : COLOR_ICON;
    if (!item->enabled) {
        color = with_alpha(color, COLOR_DISABLED_ALPHA >> 24);
    }
    icon_draw(canvas, item->icon, (float)(rect->left + rect->right) * 0.5f,
              (float)(rect->top + rect->bottom) * 0.5f, (float)ui_px(scale, ICON_SIZE), color);
    if (item->hint) {
        draw_hint(canvas, rect, item->hint, scale);
    }
}

/**
 * @brief Paints a text button; selected buttons use the primary color.
 * @param canvas Target canvas.
 * @param item Button item.
 * @param rect Button rectangle in canvas pixels.
 * @param state Interaction state.
 * @param scale Scale factor.
 */
static void paint_text_button(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state,
                              float scale)
{
    float left = (float)rect->left;
    float top = (float)rect->top;
    float width = (float)(rect->right - rect->left);
    float height = (float)(rect->bottom - rect->top);
    float radius = (float)ui_px(scale, BUTTON_RADIUS);
    Color text = COLOR_ICON;
    if (item->selected) {
        Color base = item->color ? item->color : COLOR_PRIMARY;
        Color fill = state.pressed ? shade(base, 0.85f) : (state.hover ? shade(base, 1.12f) : base);
        ui_fill_round_rect(canvas, left, top, width, height, radius, fill);
        text = item->color ? 0xFFFFFFFFu : COLOR_KNOB_ON;
    } else {
        if (state.pressed) {
            ui_fill_round_rect(canvas, left, top, width, height, radius, COLOR_PRESSED);
        } else if (state.hover) {
            ui_fill_round_rect(canvas, left, top, width, height, radius, COLOR_ISLAND_HOVER);
        }
        ui_stroke_round_rect(canvas, left + 0.5f, top + 0.5f, width - 1.0f, height - 1.0f, radius - 0.5f,
                             1.0f, COLOR_DIVIDER);
    }
    draw_label_centered(canvas, item->text, ui_px(scale, TITLE_FONT_SIZE + 1), rect, text);
}

/**
 * @brief Blends an opaque color over an opaque pixel of the canvas.
 * @param canvas Target canvas.
 * @param x Pixel x in canvas coordinates.
 * @param y Pixel y in canvas coordinates.
 * @param color Opaque source color.
 * @param coverage 0 to 1 share of the source.
 */
static void blend_pixel(Canvas *canvas, int x, int y, Color color, float coverage)
{
    if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height || coverage <= 0.0f) {
        return;
    }
    uint32_t *pixel = &canvas->pixels[(size_t)y * (size_t)canvas->width + (size_t)x];
    uint32_t result = 0;
    for (int shift = 0; shift <= 24; shift += 8) {
        float source = shift == 24 ? 255.0f : (float)((color >> shift) & 0xFFu);
        float below = (float)((*pixel >> shift) & 0xFFu);
        result |= (uint32_t)lroundf(below + (source - below) * fminf(coverage, 1.0f)) << shift;
    }
    *pixel = result;
}

/**
 * @brief Draws a handle ring (dark outline, light ring) around a point.
 * @param canvas Target canvas.
 * @param x Center x.
 * @param y Center y.
 * @param radius Outer radius of the light ring.
 * @param fill Color shown inside the ring.
 */
static void draw_marker(Canvas *canvas, float x, float y, float radius, Color fill)
{
    ui_fill_circle(canvas, x, y, radius + 1.0f, 0x99000000u);
    ui_fill_circle(canvas, x, y, radius, 0xFFFFFFFFu);
    ui_fill_circle(canvas, x, y, radius - 2.0f, fill);
}

/**
 * @brief Paints the color picker button: a rainbow ring around the current color.
 * @param canvas Target canvas.
 * @param item Picker item.
 * @param rect Item rectangle in canvas pixels.
 * @param state Interaction state.
 * @param scale Scale factor.
 */
static void paint_picker(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state, float scale)
{
    float cx = (float)(rect->left + rect->right) * 0.5f;
    float cy = (float)(rect->top + rect->bottom) * 0.5f;
    float half = (float)ui_px(scale, SWATCH_SIZE) * 0.5f;
    float radius = (float)ui_px(scale, SWATCH_RADIUS);
    float rim = 4.0f * scale;
    for (int y = rect->top; y < rect->bottom; y++) {
        for (int x = rect->left; x < rect->right; x++) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float qx = fabsf(dx) - (half - radius);
            float qy = fabsf(dy) - (half - radius);
            float outside = sqrtf(fmaxf(qx, 0.0f) * fmaxf(qx, 0.0f) + fmaxf(qy, 0.0f) * fmaxf(qy, 0.0f)) +
                            fminf(fmaxf(qx, qy), 0.0f) - radius;
            float coverage = fminf(fmaxf(0.5f - outside, 0.0f), 1.0f);
            if (coverage > 0.0f) {
                float hue = atan2f(dy, dx) / 6.2831853f;
                blend_pixel(canvas, x, y, color_from_hsv(hue, 0.85f, 1.0f), coverage);
            }
        }
    }
    float inner = half - rim;
    ui_fill_round_rect(canvas, cx - inner, cy - inner, 2.0f * inner, 2.0f * inner,
                       fmaxf(radius - rim * 0.5f, 1.0f), item->color);
    if (item->selected) {
        draw_ring(canvas, rect, 2.0f * scale, COLOR_ACTIVE_BORDER, scale);
    } else if (state.hover || state.pressed) {
        draw_ring(canvas, rect, 1.0f * scale, COLOR_SWATCH_HOVER, scale);
    }
}

/**
 * @brief Paints the saturation and brightness square of the color picker.
 * @param canvas Target canvas.
 * @param item Square item; hue is in item->hue, saturation in value, brightness in value2.
 * @param rect Item rectangle in canvas pixels.
 * @param scale Scale factor.
 */
static void paint_sv(Canvas *canvas, const UiItem *item, const RECT *outer, float scale)
{
    int margin = ui_px(scale, UI_PICKER_MARGIN);
    RECT inner = {outer->left + margin, outer->top + margin, outer->right - margin, outer->bottom - margin};
    const RECT *rect = &inner;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    for (int y = 0; y < height; y++) {
        float brightness = 1.0f - (float)y / (float)(height - 1);
        for (int x = 0; x < width; x++) {
            Color color = color_from_hsv(item->hue, (float)x / (float)(width - 1), brightness);
            blend_pixel(canvas, rect->left + x, rect->top + y, color, 1.0f);
        }
    }
    ui_stroke_round_rect(canvas, (float)rect->left + 0.5f, (float)rect->top + 0.5f, (float)width - 1.0f,
                         (float)height - 1.0f, 0.0f, 1.0f, COLOR_SWATCH_BORDER);
    float x = (float)rect->left + item->value * (float)(width - 1) + 0.5f;
    float y = (float)rect->top + (1.0f - item->value2) * (float)(height - 1) + 0.5f;
    draw_marker(canvas, x, y, 7.0f * scale, color_from_hsv(item->hue, item->value, item->value2));
}

/**
 * @brief Paints the hue strip of the color picker.
 * @param canvas Target canvas.
 * @param item Strip item; the hue is in item->value.
 * @param rect Item rectangle in canvas pixels.
 * @param scale Scale factor.
 */
static void paint_hue(Canvas *canvas, const UiItem *item, const RECT *outer, float scale)
{
    int margin = ui_px(scale, UI_PICKER_MARGIN);
    RECT inner = {outer->left + margin, outer->top + margin, outer->right - margin, outer->bottom - margin};
    const RECT *rect = &inner;
    int width = rect->right - rect->left;
    for (int x = 0; x < width; x++) {
        Color color = color_from_hsv((float)x / (float)(width - 1), 1.0f, 1.0f);
        for (int y = rect->top; y < rect->bottom; y++) {
            blend_pixel(canvas, rect->left + x, y, color, 1.0f);
        }
    }
    float x = (float)rect->left + item->value * (float)(width - 1) + 0.5f;
    draw_marker(canvas, x, (float)(rect->top + rect->bottom) * 0.5f, 8.0f * scale,
                color_from_hsv(item->value, 1.0f, 1.0f));
}

/**
 * @brief Paints the editable hex value field of the color picker.
 * @param canvas Target canvas.
 * @param item Field item; selected while it is being typed into.
 * @param rect Item rectangle in canvas pixels.
 * @param state Interaction state.
 * @param scale Scale factor.
 */
static void paint_hex(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state, float scale)
{
    float left = (float)rect->left;
    float top = (float)rect->top;
    float width = (float)(rect->right - rect->left);
    float height = (float)(rect->bottom - rect->top);
    float radius = (float)ui_px(scale, BUTTON_RADIUS);
    if (state.hover && !item->selected) {
        ui_fill_round_rect(canvas, left, top, width, height, radius, COLOR_ISLAND_HOVER);
    }
    ui_stroke_round_rect(canvas, left + 0.5f, top + 0.5f, width - 1.0f, height - 1.0f, radius - 0.5f,
                         item->selected ? 2.0f : 1.0f, item->selected ? COLOR_ACTIVE_BORDER : COLOR_DIVIDER);
    RECT text = {rect->left + ui_px(scale, 10), rect->top, rect->right - ui_px(scale, 10), rect->bottom};
    draw_label(canvas, item->text, ui_px(scale, 14), &text, false, COLOR_ICON);
}

/**
 * @brief Paints the title band at the top of a dialog, rounded like the dialog body.
 * @param canvas Target canvas.
 * @param rect Band rectangle in canvas pixels.
 * @param scale Scale factor.
 */
static void paint_band(Canvas *canvas, const RECT *rect, float scale)
{
    float radius = (float)ui_px(scale, (int)ISLAND_RADIUS);
    float width = (float)(rect->right - rect->left);
    float height = (float)(rect->bottom - rect->top);
    ui_fill_round_rect(canvas, (float)rect->left, (float)rect->top, width, height + radius, radius,
                       COLOR_HEADER);
    RECT below = {rect->left, rect->bottom, rect->right, rect->bottom + (int)ceilf(radius)};
    canvas_fill(canvas, &below, COLOR_ISLAND);
}

/**
 * @brief Paints the pressure toggle switch.
 * @param canvas Target canvas.
 * @param item Switch item.
 * @param rect Switch rectangle in canvas pixels.
 * @param state Interaction state.
 */
static void paint_switch(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state)
{
    float width = (float)(rect->right - rect->left);
    float height = (float)(rect->bottom - rect->top);
    bool on = item->value > 0.5f;
    Color track = on ? COLOR_PRIMARY : (state.hover ? COLOR_TRACK_HOVER : COLOR_TRACK);
    ui_fill_round_rect(canvas, (float)rect->left, (float)rect->top, width, height, height * 0.5f, track);
    float knob = height * 0.5f - height * 0.15f;
    float cx = on ? (float)rect->right - height * 0.5f : (float)rect->left + height * 0.5f;
    ui_fill_circle(canvas, cx, (float)rect->top + height * 0.5f, knob, on ? COLOR_KNOB_ON : COLOR_KNOB_OFF);
}

/**
 * @brief Paints the opacity slider.
 * @param canvas Target canvas.
 * @param item Slider item.
 * @param rect Slider rectangle in canvas pixels.
 * @param state Interaction state.
 * @param scale Scale factor.
 */
static void paint_slider(Canvas *canvas, const UiItem *item, const RECT *rect, ItemState state, float scale)
{
    float thumb = (float)ui_px(scale, SLIDER_THUMB_RADIUS);
    float trackHeight = (float)ui_px(scale, SLIDER_TRACK_HEIGHT);
    float left = (float)rect->left + thumb;
    float span = (float)(rect->right - rect->left) - 2.0f * thumb;
    float cy = (float)(rect->top + rect->bottom) * 0.5f;
    float thumbX = left + span * item->value;
    ui_fill_round_rect(canvas, left, cy - trackHeight * 0.5f, span, trackHeight, trackHeight * 0.5f,
                       COLOR_TRACK);
    if (thumbX > left) {
        ui_fill_round_rect(canvas, left, cy - trackHeight * 0.5f, thumbX - left, trackHeight,
                           trackHeight * 0.5f, COLOR_PRIMARY);
    }
    if (state.hover || state.pressed) {
        ui_fill_circle(canvas, thumbX, cy, thumb + (float)ui_px(scale, 3), with_alpha(COLOR_PRIMARY, 0x40));
    }
    ui_fill_circle(canvas, thumbX, cy, thumb, COLOR_ICON_SELECTED);
}

void ui_paint_body(Canvas *canvas, int margin, int width, int height, float radius, float scale)
{
    static const struct {
        float blur;
        float offset;
        unsigned alpha;
    } layers[3] = {{3.0f, 0.0f, 0x50}, {10.0f, 2.0f, 0x44}, {30.0f, 7.0f, 0x3C}};
    RECT all = {0, 0, canvas->width, canvas->height};
    canvas_fill(canvas, &all, 0);
    for (int i = 0; i < 3; i++) {
        Mask *mask = ui_scratch_mask(0, 0, canvas->width, canvas->height);
        if (!mask) {
            break;
        }
        mask_add_rect_shadow(mask, (float)margin, (float)margin + layers[i].offset * scale, (float)width,
                             (float)height, radius, layers[i].blur * scale);
        mask_blend(canvas, mask, layers[i].alpha << 24);
    }
    ui_fill_round_rect(canvas, (float)margin, (float)margin, (float)width, (float)height, radius,
                       COLOR_ISLAND);
}

void ui_paint_item(Canvas *canvas, int margin, const UiItem *item, ItemState state, float scale)
{
    RECT rect = {item->rect.left + margin, item->rect.top + margin, item->rect.right + margin,
                 item->rect.bottom + margin};
    if (item->kind != ITEM_BAND) {
        canvas_fill(canvas, &rect, item->onBand ? COLOR_HEADER : COLOR_ISLAND);
    }
    switch (item->kind) {
    case ITEM_BUTTON:
        paint_button(canvas, item, &rect, state, scale);
        break;
    case ITEM_GRIP:
        icon_draw(canvas, ICON_GRIP, (float)(rect.left + rect.right) * 0.5f,
                  (float)(rect.top + rect.bottom) * 0.5f, (float)ui_px(scale, ICON_SIZE),
                  state.hover ? COLOR_GRIP_HOVER : COLOR_GRIP);
        break;
    case ITEM_DIVIDER:
        canvas_fill(canvas, &rect, COLOR_DIVIDER);
        break;
    case ITEM_LABEL:
        draw_label(canvas, item->text, ui_px(scale, item->value > 0.0f ? (int)item->value : TITLE_FONT_SIZE),
                   &rect, item->alignRight, item->color ? item->color : COLOR_TITLE);
        break;
    case ITEM_SWATCH:
        draw_swatch(canvas, &rect, item->color, item->checker, scale);
        if (item->selected) {
            draw_ring(canvas, &rect, 2.0f * scale, COLOR_ACTIVE_BORDER, scale);
        } else if (state.hover || state.pressed) {
            draw_ring(canvas, &rect, 1.0f * scale, COLOR_SWATCH_HOVER, scale);
        }
        break;
    case ITEM_CURRENT_SWATCH:
        draw_swatch(canvas, &rect, item->color, false, scale);
        break;
    case ITEM_SWITCH:
        paint_switch(canvas, item, &rect, state);
        break;
    case ITEM_SLIDER:
        paint_slider(canvas, item, &rect, state, scale);
        break;
    case ITEM_ICON:
        icon_draw(canvas, item->icon, (float)(rect.left + rect.right) * 0.5f,
                  (float)(rect.top + rect.bottom) * 0.5f, (float)ui_px(scale, ICON_SIZE), COLOR_ICON);
        break;
    case ITEM_TEXT_BUTTON:
        paint_text_button(canvas, item, &rect, state, scale);
        break;
    case ITEM_PICKER:
        paint_picker(canvas, item, &rect, state, scale);
        break;
    case ITEM_SV:
        paint_sv(canvas, item, &rect, scale);
        break;
    case ITEM_HUE:
        paint_hue(canvas, item, &rect, scale);
        break;
    case ITEM_HEX:
        paint_hex(canvas, item, &rect, state, scale);
        break;
    case ITEM_BAND:
        paint_band(canvas, &rect, scale);
        break;
    }
}

void ui_paint_handle(Canvas *canvas, int margin, PanelId panel, const UiModel *model, bool hover, float scale)
{
    int size = ui_px(scale, UI_HANDLE_SIZE);
    if (hover) {
        ui_fill_round_rect(canvas, (float)margin, (float)margin, (float)size, (float)size,
                           (float)ui_px(scale, HANDLE_RADIUS), COLOR_ISLAND_HOVER);
    }
    float cx = (float)margin + (float)size * 0.5f;
    float cy = cx;
    float iconSize = (float)ui_px(scale, ICON_SIZE);
    if (panel == PANEL_TOOLBAR) {
        Icon icon = model->drawing ? icon_for_tool(model->tool) : ICON_DRAWING_OFF;
        icon_draw(canvas, icon, cx, cy, iconSize, COLOR_ICON);
    } else if (panel == PANEL_PROPERTIES) {
        float chip = (float)ui_px(scale, HANDLE_CHIP);
        float chipLeft = (float)margin + ((float)size - chip) * 0.5f;
        float chipRadius = (float)ui_px(scale, 4);
        ui_fill_round_rect(canvas, chipLeft, chipLeft, chip, chip, chipRadius, model->style.stroke);
        ui_stroke_round_rect(canvas, chipLeft + 0.5f, chipLeft + 0.5f, chip - 1.0f, chip - 1.0f,
                             chipRadius - 0.5f, 1.0f, COLOR_SWATCH_BORDER);
    } else {
        Color color = model->canUndo ? COLOR_ICON : with_alpha(COLOR_ICON, COLOR_DISABLED_ALPHA >> 24);
        icon_draw(canvas, ICON_UNDO, cx, cy, iconSize, color);
    }
}

unsigned ui_handle_signature(PanelId panel, const UiModel *model)
{
    if (panel == PANEL_TOOLBAR) {
        return ((unsigned)model->tool + 1u) | (model->drawing ? 0x100u : 0u);
    }
    if (panel == PANEL_PROPERTIES) {
        return model->style.stroke;
    }
    return model->canUndo ? 1u : 0u;
}
