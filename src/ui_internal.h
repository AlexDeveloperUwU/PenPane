/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file ui_internal.h
 * @brief Private interfaces shared by the UI implementation files: layout items, layout building
 *        and painting. Not part of the public UI API.
 */
#ifndef PENPANE_UI_INTERNAL_H
#define PENPANE_UI_INTERNAL_H

#include <stdbool.h>
#include <windows.h>

#include "canvas.h"
#include "icons.h"
#include "ui.h"

#define UI_MAX_ITEMS 64
#define UI_ITEM_TEXT_CHARS 16
#define UI_SHADOW_MARGIN 20
#define UI_HANDLE_SIZE 28
#define UI_DRAG_STRIP 24
#define UI_SLIDER_STEPS 10

/** @brief Kinds of visual elements inside an island. */
typedef enum ItemKind {
    ITEM_BUTTON,
    ITEM_SWATCH,
    ITEM_CURRENT_SWATCH,
    ITEM_DIVIDER,
    ITEM_LABEL,
    ITEM_GRIP,
    ITEM_SWITCH,
    ITEM_SLIDER
} ItemKind;

/** @brief One element of an island with its geometry, look and the event it produces. */
typedef struct UiItem {
    ItemKind kind;
    RECT rect;
    Icon icon;
    wchar_t hint;
    Color color;
    bool selected;
    bool enabled;
    bool checker;
    bool alignRight;
    float value;
    wchar_t text[UI_ITEM_TEXT_CHARS];
    UiEvent event;
} UiItem;

/** @brief All elements of one island plus its content size; coordinates are relative to the island. */
typedef struct UiLayout {
    UiItem items[UI_MAX_ITEMS];
    int count;
    int width;
    int height;
} UiLayout;

/** @brief Interaction state used when painting an item. */
typedef struct ItemState {
    bool hover;
    bool pressed;
} ItemState;

/**
 * @brief Scales a design pixel value.
 * @param scale Scale factor, 1.0 at 96 DPI.
 * @param value Value at 100 percent.
 * @return Rounded device pixels.
 */
int ui_px(float scale, int value);

/**
 * @brief Builds the layout of one island for a model.
 * @param panel Island to lay out.
 * @param model Application state.
 * @param pinned true when the island is pinned open; selects the look of its pin button.
 * @param scale Scale factor.
 * @param layout Receives the layout; fully overwritten.
 */
void ui_layout_build(PanelId panel, const UiModel *model, bool pinned, float scale, UiLayout *layout);

/**
 * @brief Compares two items for visual and behavioral equality.
 * @param first First item.
 * @param second Second item.
 * @return true when both render and behave identically.
 */
bool ui_item_equal(const UiItem *first, const UiItem *second);

/**
 * @brief Finds the interactive item under a point.
 * @param layout Layout to search.
 * @param x Point x relative to the island content.
 * @param y Point y relative to the island content.
 * @return Item index, or -1 when the point is on the island background or a passive element.
 */
int ui_layout_hit(const UiLayout *layout, int x, int y);

/**
 * @brief Converts a slider pointer position to a stepped value.
 * @param item Slider item.
 * @param x Pointer x relative to the island content.
 * @param scale Scale factor.
 * @return Value from 0 to 1 in steps of one tenth.
 */
float ui_slider_value_at(const UiItem *item, int x, float scale);

/**
 * @brief Paints shadow and body of an island of the given content size onto a cleared canvas.
 * @param canvas Target canvas, cleared by this function.
 * @param margin Shadow margin around the content in pixels.
 * @param width Content width.
 * @param height Content height.
 * @param radius Corner radius.
 * @param scale Scale factor.
 */
void ui_paint_body(Canvas *canvas, int margin, int width, int height, float radius, float scale);

/**
 * @brief Paints one item, first clearing its rectangle to the island color.
 * @param canvas Target canvas.
 * @param margin Shadow margin, the offset of island content inside the canvas.
 * @param item Item to paint.
 * @param state Hover and pressed flags.
 * @param scale Scale factor.
 */
void ui_paint_item(Canvas *canvas, int margin, const UiItem *item, ItemState state, float scale);

/**
 * @brief Paints the collapsed handle contents onto an already painted body.
 * @param canvas Target canvas.
 * @param margin Shadow margin.
 * @param panel Island the handle belongs to.
 * @param model Application state used for the glyph.
 * @param hover true while the pointer is over the handle.
 * @param scale Scale factor.
 */
void ui_paint_handle(Canvas *canvas, int margin, PanelId panel, const UiModel *model, bool hover,
                     float scale);

/**
 * @brief Computes a value that changes whenever the handle glyph of an island changes.
 * @param panel Island.
 * @param model Application state.
 * @return Signature of the visible glyph state.
 */
unsigned ui_handle_signature(PanelId panel, const UiModel *model);

#endif
