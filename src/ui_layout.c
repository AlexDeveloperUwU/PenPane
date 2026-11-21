/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

#define SWATCH_CELL 26
#define SWATCH_GAP 5
#define SECTION_PAD 12
#define SECTION_INNER 187
#define SECTION_GAP 14
#define TITLE_HEIGHT 16
#define TITLE_GAP 6
#define SMALL_BUTTON 32
#define SMALL_BUTTON_GAP 6
#define SWITCH_WIDTH 36
#define SWITCH_HEIGHT 20
#define SLIDER_HEIGHT 22
#define SLIDER_THUMB_RADIUS 7
#define GRIP_WIDTH 18
#define BUTTON_GAP 2
#define DIVIDER_SIDE 4
#define DIVIDER_HEIGHT 24
#define HEADER_GAP 8

static const wchar_t TOOL_HINTS[TOOL_COUNT] = {L'1', L'2', L'4', L'5', L'6', L'7', L'K', L'8', L'0'};

int ui_px(float scale, int value)
{
    return (int)lroundf((float)value * scale);
}

/**
 * @brief Appends a zero-initialized item to a layout.
 * @param layout Layout to extend.
 * @param kind Item kind.
 * @param x Left edge.
 * @param y Top edge.
 * @param width Width.
 * @param height Height.
 * @return New item; a discarded sink item when the layout is full.
 */
static UiItem *item_add(UiLayout *layout, ItemKind kind, int x, int y, int width, int height)
{
    static UiItem sink;
    UiItem *item = layout->count < UI_MAX_ITEMS ? &layout->items[layout->count++] : &sink;
    memset(item, 0, sizeof *item);
    item->kind = kind;
    item->enabled = true;
    item->rect.left = x;
    item->rect.top = y;
    item->rect.right = x + width;
    item->rect.bottom = y + height;
    return item;
}

/**
 * @brief Adds a toolbar style icon button that emits an action event.
 * @param layout Layout to extend.
 * @param x Left edge.
 * @param y Top edge.
 * @param size Edge length.
 * @param icon Icon to show.
 * @param action Action emitted on click.
 * @return The new button.
 */
static UiItem *button_action(UiLayout *layout, int x, int y, int size, Icon icon, Action action)
{
    UiItem *item = item_add(layout, ITEM_BUTTON, x, y, size, size);
    item->icon = icon;
    item->event.kind = UI_EVENT_ACTION;
    item->event.action = action;
    return item;
}

/**
 * @brief Adds a vertical divider and returns the x position after it.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param x Left edge before the divider.
 * @param y Top of the row the divider sits in.
 * @param rowHeight Height of the row.
 * @return x position where the next element starts.
 */
static int divider_add(UiLayout *layout, float scale, int x, int y, int rowHeight)
{
    int width = ui_px(scale, 1) > 0 ? ui_px(scale, 1) : 1;
    int height = ui_px(scale, DIVIDER_HEIGHT);
    int left = x + ui_px(scale, DIVIDER_SIDE);
    item_add(layout, ITEM_DIVIDER, left, y + (rowHeight - height) / 2, width, height);
    return left + width + ui_px(scale, DIVIDER_SIDE) + ui_px(scale, BUTTON_GAP);
}

/**
 * @brief Adds the drag grip and the pin toggle that start the header of every island.
 * @param layout Layout to extend.
 * @param panel Island being built; reported by the pin event.
 * @param pinned true when the island is pinned open.
 * @param scale Scale factor.
 * @param x Left edge of the grip.
 * @param y Top of the row.
 * @return x position after the pin button including the gap.
 */
static int header_add(UiLayout *layout, PanelId panel, bool pinned, float scale, int x, int y)
{
    int button = ui_px(scale, BUTTON_SIZE);
    int gap = ui_px(scale, BUTTON_GAP);
    item_add(layout, ITEM_GRIP, x, y, ui_px(scale, GRIP_WIDTH), button)->icon = ICON_GRIP;
    x += ui_px(scale, GRIP_WIDTH) + gap;
    UiItem *pin = item_add(layout, ITEM_BUTTON, x, y, button, button);
    pin->icon = pinned ? ICON_PIN : ICON_PIN_OFF;
    pin->selected = pinned;
    pin->event.kind = UI_EVENT_PANEL_PINNED;
    pin->event.panel = panel;
    pin->event.index = pinned ? 0 : 1;
    return x + button + gap;
}

/**
 * @brief Builds the toolbar island.
 * @param layout Layout to fill.
 * @param model Application state.
 * @param pinned true when the toolbar is pinned open.
 * @param scale Scale factor.
 */
static void build_toolbar(UiLayout *layout, const UiModel *model, bool pinned, float scale)
{
    int pad = ui_px(scale, ISLAND_PADDING);
    int button = ui_px(scale, BUTTON_SIZE);
    int gap = ui_px(scale, BUTTON_GAP);
    int y = pad;
    int x = header_add(layout, PANEL_TOOLBAR, pinned, scale, pad, y);

    x = divider_add(layout, scale, x, y, button);

    UiItem *lock = button_action(layout, x, y, button, model->toolLocked ? ICON_LOCK_CLOSED : ICON_LOCK_OPEN,
                                 ACTION_TOGGLE_TOOL_LOCK);
    lock->selected = model->toolLocked;
    lock->hint = L'Q';
    x += button + gap;

    x = divider_add(layout, scale, x, y, button);

    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        UiItem *item = item_add(layout, ITEM_BUTTON, x, y, button, button);
        item->icon = icon_for_tool(tool);
        item->hint = TOOL_HINTS[tool];
        item->selected = model->tool == (Tool)tool;
        item->event.kind = UI_EVENT_SELECT_TOOL;
        item->event.tool = (Tool)tool;
        x += button + gap;
    }

    x = divider_add(layout, scale, x, y, button);

    UiItem *mode = button_action(layout, x, y, button,
                                 model->whiteboard ? ICON_MODE_WHITEBOARD : ICON_MODE_DESKTOP,
                                 ACTION_TOGGLE_MODE);
    mode->selected = model->whiteboard;
    x += button + gap;

    UiItem *drawing = button_action(layout, x, y, button,
                                    model->drawing ? ICON_DRAWING_ON : ICON_DRAWING_OFF,
                                    ACTION_TOGGLE_OVERLAY);
    drawing->selected = model->drawing;
    x += button + gap;

    button_action(layout, x, y, button, ICON_EXIT, ACTION_EMERGENCY_EXIT);
    x += button;

    layout->width = x + pad;
    layout->height = button + 2 * pad;
}

/**
 * @brief Builds the history island.
 * @param layout Layout to fill.
 * @param model Application state.
 * @param pinned true when the history island is pinned open.
 * @param scale Scale factor.
 */
static void build_history(UiLayout *layout, const UiModel *model, bool pinned, float scale)
{
    int pad = ui_px(scale, ISLAND_PADDING);
    int button = ui_px(scale, BUTTON_SIZE);
    int gap = ui_px(scale, BUTTON_GAP);
    int y = pad;
    int x = header_add(layout, PANEL_HISTORY, pinned, scale, pad, y);

    x = divider_add(layout, scale, x, y, button);
    button_action(layout, x, y, button, ICON_UNDO, ACTION_UNDO)->enabled = model->canUndo;
    x += button + gap;
    button_action(layout, x, y, button, ICON_REDO, ACTION_REDO)->enabled = model->canRedo;
    x += button;

    layout->width = x + pad;
    layout->height = button + 2 * pad;
}

/**
 * @brief Adds a section title and returns the y position of the section content.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the title.
 * @param text Title text.
 * @return y of the content below the title.
 */
static int title_add(UiLayout *layout, float scale, int y, const wchar_t *text)
{
    UiItem *item = item_add(layout, ITEM_LABEL, ui_px(scale, SECTION_PAD), y,
                            ui_px(scale, SECTION_INNER), ui_px(scale, TITLE_HEIGHT));
    wcsncpy(item->text, text, UI_ITEM_TEXT_CHARS - 1);
    return y + ui_px(scale, TITLE_HEIGHT + TITLE_GAP);
}

/**
 * @brief Adds a row of preset color swatches.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the row.
 * @param palette Preset colors.
 * @param count Number of presets.
 * @param selected Index of the selected preset, or -1.
 * @param kind Event kind emitted when a swatch is chosen.
 */
static void swatches_add(UiLayout *layout, float scale, int y, const Color *palette, int count,
                         int selected, UiEventKind kind)
{
    int cell = ui_px(scale, SWATCH_CELL);
    for (int i = 0; i < count; i++) {
        int x = ui_px(scale, SECTION_PAD) + i * (cell + ui_px(scale, SWATCH_GAP));
        UiItem *item = item_add(layout, ITEM_SWATCH, x, y, cell, cell);
        item->color = palette[i];
        item->checker = (palette[i] >> 24) == 0;
        item->selected = i == selected;
        item->event.kind = kind;
        item->event.index = i;
    }
}

/**
 * @brief Adds a row of three icon buttons that choose one value.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the row.
 * @param icons Icons of the three buttons.
 * @param selected Index of the selected button.
 * @param kind Event kind emitted when a button is chosen.
 */
static void choices_add(UiLayout *layout, float scale, int y, const Icon icons[3], int selected,
                        UiEventKind kind)
{
    int size = ui_px(scale, SMALL_BUTTON);
    for (int i = 0; i < 3; i++) {
        int x = ui_px(scale, SECTION_PAD) + i * (size + ui_px(scale, SMALL_BUTTON_GAP));
        UiItem *item = item_add(layout, ITEM_BUTTON, x, y, size, size);
        item->icon = icons[i];
        item->selected = i == selected;
        item->event.kind = kind;
        item->event.index = i;
    }
}

/**
 * @brief Builds the properties island according to the visible sections.
 * @param layout Layout to fill.
 * @param model Application state.
 * @param pinned true when the properties island is pinned open.
 * @param scale Scale factor.
 */
static void build_properties(UiLayout *layout, const UiModel *model, bool pinned, float scale)
{
    static const Icon widthIcons[3] = {ICON_WIDTH_THIN, ICON_WIDTH_MEDIUM, ICON_WIDTH_BOLD};
    static const Icon styleIcons[3] = {ICON_STYLE_SOLID, ICON_STYLE_DASHED, ICON_STYLE_DOTTED};
    int pad = ui_px(scale, SECTION_PAD);
    int gap = ui_px(scale, SECTION_GAP);
    int cell = ui_px(scale, SWATCH_CELL);
    int edge = ui_px(scale, ISLAND_PADDING);
    header_add(layout, PANEL_PROPERTIES, pinned, scale, edge, edge);
    int headerCount = layout->count;
    int y = edge + ui_px(scale, BUTTON_SIZE) + ui_px(scale, HEADER_GAP);
    const Style *style = &model->style;

    if (model->sections & UI_SECTION_STROKE) {
        y = title_add(layout, scale, y, L"Stroke");
        swatches_add(layout, scale, y, STROKE_PALETTE, STROKE_PALETTE_SIZE,
                     style_stroke_index(style->stroke), UI_EVENT_SET_STROKE);
        int after = pad + STROKE_PALETTE_SIZE * (cell + ui_px(scale, SWATCH_GAP));
        int width = ui_px(scale, 1) > 0 ? ui_px(scale, 1) : 1;
        item_add(layout, ITEM_DIVIDER, after, y + (cell - ui_px(scale, 20)) / 2, width,
                 ui_px(scale, 20));
        UiItem *current = item_add(layout, ITEM_CURRENT_SWATCH, after + width + ui_px(scale, SWATCH_GAP),
                                   y, cell, cell);
        current->color = style->stroke;
        y += cell + gap;
    }
    if (model->sections & UI_SECTION_FILL) {
        y = title_add(layout, scale, y, L"Background");
        swatches_add(layout, scale, y, FILL_PALETTE, FILL_PALETTE_SIZE,
                     style_fill_index(style->fill), UI_EVENT_SET_FILL);
        y += cell + gap;
    }
    if (model->sections & UI_SECTION_WIDTH) {
        y = title_add(layout, scale, y, L"Stroke width");
        choices_add(layout, scale, y, widthIcons, style_width_index(style->width), UI_EVENT_SET_WIDTH);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_STROKE_STYLE) {
        y = title_add(layout, scale, y, L"Stroke style");
        choices_add(layout, scale, y, styleIcons, (int)style->strokeStyle, UI_EVENT_SET_STROKE_STYLE);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_PRESSURE) {
        y = title_add(layout, scale, y, L"Pressure");
        UiItem *toggle = item_add(layout, ITEM_SWITCH, pad, y, ui_px(scale, SWITCH_WIDTH),
                                  ui_px(scale, SWITCH_HEIGHT));
        toggle->value = style->pressure ? 1.0f : 0.0f;
        toggle->event.kind = UI_EVENT_SET_PRESSURE;
        toggle->event.index = style->pressure ? 0 : 1;
        y += ui_px(scale, SWITCH_HEIGHT) + gap;
    }
    if (model->sections & UI_SECTION_OPACITY) {
        float opacity = roundf(fminf(fmaxf(style->opacity, 0.0f), 1.0f) * UI_SLIDER_STEPS) / UI_SLIDER_STEPS;
        UiItem *label = layout->count < UI_MAX_ITEMS ? &layout->items[layout->count] : NULL;
        y = title_add(layout, scale, y, L"Opacity");
        if (label) {
            UiItem *value = item_add(layout, ITEM_LABEL, label->rect.left, label->rect.top,
                                     ui_px(scale, SECTION_INNER), ui_px(scale, TITLE_HEIGHT));
            value->alignRight = true;
            value->color = COLOR_ICON;
            swprintf(value->text, UI_ITEM_TEXT_CHARS, L"%d", (int)lroundf(opacity * 100.0f));
        }
        UiItem *slider = item_add(layout, ITEM_SLIDER, pad, y, ui_px(scale, SECTION_INNER),
                                  ui_px(scale, SLIDER_HEIGHT));
        slider->value = opacity;
        slider->event.kind = UI_EVENT_SET_OPACITY;
        slider->event.value = opacity;
        y += ui_px(scale, SLIDER_HEIGHT) + gap;
    }
    layout->width = 2 * pad + ui_px(scale, SECTION_INNER);
    layout->height = layout->count > headerCount ? y - gap + pad : y + edge;
}

void ui_layout_build(PanelId panel, const UiModel *model, bool pinned, float scale, UiLayout *layout)
{
    memset(layout, 0, sizeof *layout);
    if (panel == PANEL_TOOLBAR) {
        build_toolbar(layout, model, pinned, scale);
    } else if (panel == PANEL_HISTORY) {
        build_history(layout, model, pinned, scale);
    } else {
        build_properties(layout, model, pinned, scale);
    }
}

bool ui_item_equal(const UiItem *first, const UiItem *second)
{
    return first->kind == second->kind && memcmp(&first->rect, &second->rect, sizeof first->rect) == 0 &&
           first->icon == second->icon && first->hint == second->hint &&
           first->color == second->color && first->selected == second->selected &&
           first->enabled == second->enabled && first->checker == second->checker &&
           first->alignRight == second->alignRight && first->value == second->value &&
           wcscmp(first->text, second->text) == 0 && first->event.kind == second->event.kind &&
           first->event.action == second->event.action && first->event.tool == second->event.tool &&
           first->event.index == second->event.index && first->event.value == second->event.value;
}

int ui_layout_hit(const UiLayout *layout, int x, int y)
{
    for (int i = layout->count - 1; i >= 0; i--) {
        const UiItem *item = &layout->items[i];
        bool interactive = item->kind == ITEM_BUTTON || item->kind == ITEM_SWATCH ||
                           item->kind == ITEM_SWITCH || item->kind == ITEM_SLIDER;
        if (interactive && x >= item->rect.left && x < item->rect.right && y >= item->rect.top &&
            y < item->rect.bottom) {
            return i;
        }
    }
    return -1;
}

float ui_slider_value_at(const UiItem *item, int x, float scale)
{
    int thumb = ui_px(scale, SLIDER_THUMB_RADIUS);
    float span = (float)(item->rect.right - item->rect.left - 2 * thumb);
    float t = span > 0.0f ? ((float)(x - item->rect.left - thumb)) / span : 0.0f;
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    return roundf(t * UI_SLIDER_STEPS) / UI_SLIDER_STEPS;
}
