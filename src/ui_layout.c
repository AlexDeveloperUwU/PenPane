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
#define OPACITY_VALUE_WIDTH 40
#define SLIDER_THUMB_RADIUS 7
#define GRIP_WIDTH 18
#define BUTTON_GAP 2
#define DIVIDER_SIDE 4
#define DIVIDER_HEIGHT 24
#define HEADER_GAP 8
#define DIALOG_PAD 22
#define DIALOG_TITLE_FONT 15
#define DIALOG_ROW_BUTTON 28
#define SETTINGS_WIDTH 420
#define SETTINGS_TABS 4
#define SHORTCUT_WIDTH 140
#define SHORTCUT_HEIGHT 24
#define SETTINGS_ROW 38
#define CONFIRM_WIDTH 420
#define PICKER_WIDTH 320
#define PICKER_SQUARE_HEIGHT 160
#define PICKER_HUE_HEIGHT 16
#define SWATCHES_PER_ROW 6

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

    UiItem *drawing = button_action(layout, x, y, button, model->drawing ? ICON_DRAWING_ON : ICON_DRAWING_OFF,
                                    ACTION_TOGGLE_OVERLAY);
    drawing->selected = model->drawing;
    x += button + gap;

    x = divider_add(layout, scale, x, y, button);

    UiItem *lock = button_action(layout, x, y, button, model->toolLocked ? ICON_LOCK_CLOSED : ICON_LOCK_OPEN,
                                 ACTION_TOGGLE_TOOL_LOCK);
    lock->selected = model->toolLocked;
    lock->hint = L'Q';
    x += button + gap;

    x = divider_add(layout, scale, x, y, button);

    Tool order[TOOL_COUNT];
    if (tool_order_valid(model->toolOrder)) {
        memcpy(order, model->toolOrder, sizeof order);
    } else {
        tool_order_default(order);
    }
    int slot = 0;
    for (int i = 0; i < TOOL_COUNT; i++) {
        Tool tool = order[i];
        if ((model->hiddenTools & (1u << tool)) != 0) {
            continue;
        }
        UiItem *item = item_add(layout, ITEM_BUTTON, x, y, button, button);
        item->icon = icon_for_tool(tool);
        item->hint = slot < 9 ? (wchar_t)(L'1' + slot) : (slot == 9 ? L'0' : 0);
        slot++;
        item->selected = model->tool == tool;
        item->event.kind = UI_EVENT_SELECT_TOOL;
        item->event.tool = tool;
        x += button + gap;
    }

    x = divider_add(layout, scale, x, y, button);

    button_action(layout, x, y, button, ICON_SETTINGS, ACTION_OPEN_SETTINGS);
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
    if (model->drawingsHidden && !model->whiteboard) {
        item_add(layout, ITEM_GRIP, pad, y, ui_px(scale, GRIP_WIDTH), button)->icon = ICON_GRIP;
        int eyeX = pad + ui_px(scale, GRIP_WIDTH) + gap;
        UiItem *eye = button_action(layout, eyeX, y, button, ICON_EYE_OFF, ACTION_TOGGLE_DRAWINGS);
        eye->selected = true;
        layout->width = eyeX + button + pad;
        layout->height = button + 2 * pad;
        return;
    }
    int x = header_add(layout, PANEL_HISTORY, pinned, scale, pad, y);

    x = divider_add(layout, scale, x, y, button);
    button_action(layout, x, y, button, ICON_UNDO, ACTION_UNDO)->enabled = model->canUndo;
    x += button + gap;
    button_action(layout, x, y, button, ICON_REDO, ACTION_REDO)->enabled = model->canRedo;
    x += button + gap;

    x = divider_add(layout, scale, x, y, button);
    button_action(layout, x, y, button, ICON_COPY, ACTION_COPY_PNG);
    x += button + gap;
    button_action(layout, x, y, button, ICON_SAVE, ACTION_SAVE_PNG);
    x += button + gap;

    x = divider_add(layout, scale, x, y, button);
    UiItem *mode =
        button_action(layout, x, y, button, model->whiteboard ? ICON_MODE_WHITEBOARD : ICON_MODE_DESKTOP,
                      ACTION_TOGGLE_MODE);
    mode->selected = model->whiteboard;
    x += button + gap;
    if (!model->whiteboard) {
        UiItem *hide = button_action(layout, x, y, button, model->drawingsHidden ? ICON_EYE_OFF : ICON_EYE,
                                     ACTION_TOGGLE_DRAWINGS);
        hide->selected = model->drawingsHidden;
        x += button + gap;
    }
    button_action(layout, x, y, button, ICON_TRASH, ACTION_ASK_CLEAR);
    x += button;

    layout->width = x + pad;
    layout->height = button + 2 * pad;
}

/**
 * @brief Adds a text label.
 * @param layout Layout to extend.
 * @param x Left edge.
 * @param y Top edge.
 * @param width Width.
 * @param height Height.
 * @param text Text to show.
 * @param fontSize Font size at 100 percent, or 0 for the default title size.
 * @param color Text color, or 0 for the title color.
 * @return The new label.
 */
static UiItem *label_add(UiLayout *layout, int x, int y, int width, int height, const wchar_t *text,
                         int fontSize, Color color)
{
    UiItem *item = item_add(layout, ITEM_LABEL, x, y, width, height);
    wcsncpy(item->text, text, UI_ITEM_TEXT_CHARS - 1);
    item->value = (float)fontSize;
    item->color = color;
    return item;
}

/**
 * @brief Adds a full-width horizontal rule to a dialog.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the rule.
 * @param width Dialog width.
 */
static void rule_add(UiLayout *layout, float scale, int y, int width)
{
    int pad = ui_px(scale, DIALOG_PAD);
    item_add(layout, ITEM_DIVIDER, pad, y, width - 2 * pad, ui_px(scale, 1) > 0 ? ui_px(scale, 1) : 1);
}

/**
 * @brief Adds the title band of a dialog: a tinted strip with the title, the close button and a rule below.
 *
 * Must be the first thing added so the band is painted before the rest.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param width Dialog width.
 * @param panel Dialog island; reported by the close button.
 * @param text Title text.
 * @return y where the dialog content starts.
 */
static int dialog_header_add(UiLayout *layout, float scale, int width, PanelId panel, const wchar_t *text)
{
    int pad = ui_px(scale, DIALOG_PAD);
    int button = ui_px(scale, BUTTON_SIZE);
    int inset = ui_px(scale, 10);
    int band = button + 2 * inset;
    item_add(layout, ITEM_BAND, 0, 0, width, band);
    label_add(layout, pad, inset, width - pad - inset - button, button, text, DIALOG_TITLE_FONT, COLOR_ICON)
        ->onBand = true;
    UiItem *close = item_add(layout, ITEM_BUTTON, width - inset - button, inset, button, button);
    close->onBand = true;
    close->icon = ICON_CLOSE;
    close->event.kind = UI_EVENT_CLOSE_DIALOG;
    close->event.panel = panel;
    item_add(layout, ITEM_DIVIDER, 0, band, width, ui_px(scale, 1) > 0 ? ui_px(scale, 1) : 1);
    return band + ui_px(scale, 20);
}

/**
 * @brief Builds the settings dialog: toolbar tools (order and visibility) and general switches.
 * @param layout Layout to fill.
 * @param model Application state.
 * @param scale Scale factor.
 */
static void build_settings(UiLayout *layout, const UiModel *model, float scale)
{
    static const wchar_t *const TOOL_LABELS[TOOL_COUNT] = {L"Select", L"Rectangle", L"Ellipse",    L"Arrow",
                                                           L"Line",   L"Pen",       L"Laser",      L"Text",
                                                           L"Eraser", L"Diamond",   L"Highlighter"};
    int width = ui_px(scale, SETTINGS_WIDTH);
    int pad = ui_px(scale, DIALOG_PAD);
    int row = ui_px(scale, SETTINGS_ROW);
    int button = ui_px(scale, DIALOG_ROW_BUTTON);
    int gap = ui_px(scale, 8);
    int switchWidth = ui_px(scale, SWITCH_WIDTH);
    int switchHeight = ui_px(scale, SWITCH_HEIGHT);
    Tool order[TOOL_COUNT];
    if (tool_order_valid(model->toolOrder)) {
        memcpy(order, model->toolOrder, sizeof order);
    } else {
        tool_order_default(order);
    }
    int visibleCount = 0;
    for (int i = 0; i < TOOL_COUNT; i++) {
        visibleCount += (model->hiddenTools & (1u << i)) == 0 ? 1 : 0;
    }
    int y = dialog_header_add(layout, scale, width, PANEL_SETTINGS, L"Settings");
    int switchX = width - pad - switchWidth;
    int downX = switchX - 2 * gap - button;
    int upX = downX - ui_px(scale, 2) - button;
    static const wchar_t *const TABS[SETTINGS_TABS] = {L"General", L"Toolbar", L"Shortcuts", L"Pen keys"};
    int tab = model->settingsTab >= 0 && model->settingsTab < SETTINGS_TABS ? model->settingsTab : 0;
    int tabWidth = (width - 2 * pad - (SETTINGS_TABS - 1) * gap) / SETTINGS_TABS;
    int tabHeight = ui_px(scale, 34);
    for (int i = 0; i < SETTINGS_TABS; i++) {
        UiItem *item = item_add(layout, ITEM_TEXT_BUTTON, pad + i * (tabWidth + gap), y, tabWidth, tabHeight);
        wcscpy(item->text, TABS[i]);
        item->selected = i == tab;
        item->event.kind = UI_EVENT_SETTINGS_TAB;
        item->event.index = i;
    }
    y += tabHeight + ui_px(scale, 18);
    int hintHeight = ui_px(scale, 34);
    int bodyHeight = y + hintHeight + TOOL_COUNT * row + pad;
    if (tab == 0) {
        static const wchar_t *const GENERAL_LABELS[6] = {
            L"Auto-hide panels",  L"Hide panels from screenshots", L"Pastel color row",
            L"Write a debug log", L"Add desktop to exports",       L"Export the full screen"};
        const bool generalValues[6] = {model->autoHide, model->excludeCapture,   model->pastel,
                                       model->debugLog, model->exportBackground, model->exportFullScreen};
        const UiEventKind generalKinds[6] = {
            UI_EVENT_SET_AUTO_HIDE, UI_EVENT_SET_EXCLUDE_CAPTURE,   UI_EVENT_SET_PASTEL,
            UI_EVENT_SET_DEBUG_LOG, UI_EVENT_SET_EXPORT_BACKGROUND, UI_EVENT_SET_EXPORT_FULL_SCREEN};
        for (int i = 0; i < 6; i++) {
            label_add(layout, pad, y, switchX - gap - pad, row, GENERAL_LABELS[i], 13, COLOR_ICON);
            UiItem *toggle = item_add(layout, ITEM_SWITCH, switchX, y + (row - switchHeight) / 2, switchWidth,
                                      switchHeight);
            toggle->value = generalValues[i] ? 1.0f : 0.0f;
            toggle->event.kind = generalKinds[i];
            toggle->event.index = generalValues[i] ? 0 : 1;
            y += row;
        }
        y += ui_px(scale, 18);
        rule_add(layout, scale, y, width);
        y += ui_px(scale, 22);
        static const wchar_t *const RESET_LABELS[2] = {L"Reset all options", L"Reset menu positions"};
        const UiEventKind resetKinds[2] = {UI_EVENT_RESET_OPTIONS, UI_EVENT_RESET_POSITIONS};
        int resetWidth = (width - 2 * pad - gap) / 2;
        int resetHeight = ui_px(scale, 38);
        for (int i = 0; i < 2; i++) {
            UiItem *reset =
                item_add(layout, ITEM_TEXT_BUTTON, pad + i * (resetWidth + gap), y, resetWidth, resetHeight);
            wcscpy(reset->text, RESET_LABELS[i]);
            reset->event.kind = resetKinds[i];
        }
    } else if (tab == 1) {
        label_add(layout, pad, y, width - 2 * pad, hintHeight, L"Order and visibility of the tools.", 12, 0);
        y += hintHeight;
        for (int i = 0; i < TOOL_COUNT; i++) {
            Tool tool = order[i];
            bool visible = (model->hiddenTools & (1u << tool)) == 0;
            item_add(layout, ITEM_ICON, pad, y, button, row)->icon = icon_for_tool(tool);
            label_add(layout, pad + button + gap, y, upX - 2 * gap - pad - button, row, TOOL_LABELS[tool], 13,
                      visible ? COLOR_ICON : 0);
            UiItem *up = item_add(layout, ITEM_BUTTON, upX, y + (row - button) / 2, button, button);
            up->icon = ICON_CHEVRON_UP;
            up->enabled = i > 0;
            up->event.kind = UI_EVENT_TOOL_MOVE;
            up->event.tool = tool;
            up->event.index = -1;
            UiItem *down = item_add(layout, ITEM_BUTTON, downX, y + (row - button) / 2, button, button);
            down->icon = ICON_CHEVRON_DOWN;
            down->enabled = i < TOOL_COUNT - 1;
            down->event.kind = UI_EVENT_TOOL_MOVE;
            down->event.tool = tool;
            down->event.index = 1;
            UiItem *toggle = item_add(layout, ITEM_SWITCH, switchX, y + (row - switchHeight) / 2, switchWidth,
                                      switchHeight);
            toggle->value = visible ? 1.0f : 0.0f;
            toggle->enabled = !(visible && visibleCount <= 1);
            toggle->event.kind = UI_EVENT_TOOL_VISIBLE;
            toggle->event.tool = tool;
            toggle->event.index = visible ? 0 : 1;
            y += row;
        }
    } else if (tab == 2) {
        if (model->learningTool >= 0) {
            int labelWidth = ui_px(scale, 44);
            label_add(layout, pad, y, labelWidth, hintHeight, L"Or pick", 12, 0);
            int keys = 6;
            int keyGap = ui_px(scale, 4);
            int keyWidth = (width - 2 * pad - labelWidth - keys * keyGap) / keys;
            for (int i = 0; i < keys; i++) {
                UiItem *key =
                    item_add(layout, ITEM_TEXT_BUTTON, pad + labelWidth + keyGap + i * (keyWidth + keyGap),
                             y + ui_px(scale, 3), keyWidth, hintHeight - ui_px(scale, 6));
                swprintf(key->text, UI_ITEM_TEXT_CHARS, L"F%d", 13 + i);
                key->event.kind = UI_EVENT_TOOL_SHORTCUT_KEY;
                key->event.tool = (Tool)model->learningTool;
                key->event.index = VK_F13 + i;
            }
        } else {
            label_add(layout, pad, y, width - 2 * pad, hintHeight, L"One extra key or pen button per tool.",
                      12, 0);
        }
        y += hintHeight;
        int clearX = width - pad - button;
        int shortcutWidth = ui_px(scale, SHORTCUT_WIDTH);
        int shortcutX = clearX - gap - shortcutWidth;
        for (int i = 0; i < TOOL_COUNT; i++) {
            Tool tool = order[i];
            item_add(layout, ITEM_ICON, pad, y, button, row)->icon = icon_for_tool(tool);
            label_add(layout, pad + button + gap, y, shortcutX - 2 * gap - pad - button, row,
                      TOOL_LABELS[tool], 13, COLOR_ICON);
            bool learning = model->learningTool == (int)tool;
            const wchar_t *shortcut = model->toolShortcut[tool];
            UiItem *extra =
                item_add(layout, ITEM_TEXT_BUTTON, shortcutX, y + (row - ui_px(scale, SHORTCUT_HEIGHT)) / 2,
                         shortcutWidth, ui_px(scale, SHORTCUT_HEIGHT));
            wcsncpy(extra->text, learning ? L"Press a button" : (shortcut[0] ? shortcut : L"Add"),
                    UI_ITEM_TEXT_CHARS - 1);
            extra->selected = learning;
            extra->event.kind = UI_EVENT_TOOL_SHORTCUT;
            extra->event.tool = tool;
            if (shortcut[0] && !learning) {
                UiItem *clear = item_add(layout, ITEM_BUTTON, clearX, y + (row - button) / 2, button, button);
                clear->icon = ICON_CLOSE;
                clear->event.kind = UI_EVENT_TOOL_SHORTCUT_CLEAR;
                clear->event.tool = tool;
            }
            y += row;
        }
    } else {
        int hintLine = ui_px(scale, 24);
        static const wchar_t *const PEN_HINTS[3] = {L"Drivers often cannot record F13 to F24.",
                                                    L"Click the driver's key box, then a key",
                                                    L"below: PenPane types it there."};
        for (int i = 0; i < 3; i++) {
            label_add(layout, pad, y, width - 2 * pad, hintLine, PEN_HINTS[i], 13, i == 1 ? COLOR_ICON : 0);
            y += hintLine;
        }
        y += ui_px(scale, 20);
        int keyWidth = (width - 2 * pad - 3 * gap) / 4;
        for (int i = 0; i < 4; i++) {
            UiItem *key =
                item_add(layout, ITEM_TEXT_BUTTON, pad + i * (keyWidth + gap), y, keyWidth, ui_px(scale, 38));
            swprintf(key->text, UI_ITEM_TEXT_CHARS, L"F%d", 13 + i);
            key->event.kind = UI_EVENT_SEND_KEY;
            key->event.index = VK_F13 + i;
        }
    }
    layout->width = width;
    layout->height = bodyHeight;
}

/**
 * @brief Builds the "clear the canvas?" confirmation.
 * @param layout Layout to fill.
 * @param scale Scale factor.
 */
static void build_confirm_clear(UiLayout *layout, float scale)
{
    int width = ui_px(scale, CONFIRM_WIDTH);
    int pad = ui_px(scale, DIALOG_PAD);
    int line = ui_px(scale, 24);
    int y = dialog_header_add(layout, scale, width, PANEL_CONFIRM, L"Clear canvas");
    label_add(layout, pad, y, width - 2 * pad, line, L"This removes every drawing in the current mode.", 13,
              COLOR_ICON);
    y += line;
    label_add(layout, pad, y, width - 2 * pad, line, L"You can bring it back with Undo.", 13, 0);
    y += line + ui_px(scale, 28);
    int buttonHeight = ui_px(scale, 38);
    int confirmWidth = ui_px(scale, 116);
    int cancelWidth = ui_px(scale, 88);
    UiItem *confirm =
        item_add(layout, ITEM_TEXT_BUTTON, width - pad - confirmWidth, y, confirmWidth, buttonHeight);
    wcscpy(confirm->text, L"Clear canvas");
    confirm->selected = true;
    confirm->color = COLOR_STROKE_RED;
    confirm->event.kind = UI_EVENT_ACTION;
    confirm->event.action = ACTION_CLEAR;
    UiItem *cancel =
        item_add(layout, ITEM_TEXT_BUTTON, width - pad - confirmWidth - ui_px(scale, 10) - cancelWidth, y,
                 cancelWidth, buttonHeight);
    wcscpy(cancel->text, L"Cancel");
    cancel->event.kind = UI_EVENT_CLOSE_DIALOG;
    cancel->event.panel = PANEL_CONFIRM;
    layout->width = width;
    layout->height = y + buttonHeight + pad;
}

/**
 * @brief Builds the stroke color picker: brightness square, hue strip and the hex value.
 * @param layout Layout to fill.
 * @param model Application state.
 * @param scale Scale factor.
 */
static void build_color_picker(UiLayout *layout, const UiModel *model, float scale)
{
    int width = ui_px(scale, PICKER_WIDTH);
    int pad = ui_px(scale, DIALOG_PAD);
    int inner = width - 2 * pad;
    int y = dialog_header_add(layout, scale, width, PANEL_COLOR, L"Stroke color");
    int margin = ui_px(scale, UI_PICKER_MARGIN);
    UiItem *square = item_add(layout, ITEM_SV, pad - margin, y, inner + 2 * margin,
                              ui_px(scale, PICKER_SQUARE_HEIGHT) + 2 * margin);
    square->hue = model->pickerHue;
    square->value = model->pickerSat;
    square->value2 = model->pickerVal;
    square->event.kind = UI_EVENT_PICK_SV;
    y = square->rect.bottom;
    UiItem *strip = item_add(layout, ITEM_HUE, pad - margin, y, inner + 2 * margin,
                             ui_px(scale, PICKER_HUE_HEIGHT) + 2 * margin);
    strip->value = model->pickerHue;
    strip->event.kind = UI_EVENT_PICK_HUE;
    y = strip->rect.bottom + ui_px(scale, 4);
    int cell = ui_px(scale, SWATCH_CELL);
    UiItem *current = item_add(layout, ITEM_CURRENT_SWATCH, pad, y, cell, cell);
    current->color = model->style.stroke;
    wchar_t hex[UI_ITEM_TEXT_CHARS];
    swprintf(hex, UI_ITEM_TEXT_CHARS, L"#%06X", (unsigned)(model->style.stroke & 0xFFFFFFu));
    UiItem *field =
        item_add(layout, ITEM_HEX, pad + cell + ui_px(scale, 10), y, inner - cell - ui_px(scale, 10), cell);
    wcscpy(field->text, hex);
    field->event.kind = UI_EVENT_EDIT_HEX;
    layout->width = width;
    layout->height = y + cell + pad;
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
    UiItem *item = item_add(layout, ITEM_LABEL, ui_px(scale, SECTION_PAD), y, ui_px(scale, SECTION_INNER),
                            ui_px(scale, TITLE_HEIGHT));
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
static void swatches_add(UiLayout *layout, float scale, int y, const Color *palette, int count, int selected,
                         UiEventKind kind)
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
 * @brief Adds color swatches that report their own color, wrapped into rows.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the first row.
 * @param colors Colors to show.
 * @param count Number of colors.
 * @param selected Currently chosen stroke color.
 * @return y below the last row.
 */
static int color_rows_add(UiLayout *layout, float scale, int y, const Color *colors, int count,
                          Color selected)
{
    int cell = ui_px(scale, SWATCH_CELL);
    int step = cell + ui_px(scale, SWATCH_GAP);
    for (int i = 0; i < count; i++) {
        UiItem *item =
            item_add(layout, ITEM_SWATCH, ui_px(scale, SECTION_PAD) + (i % SWATCHES_PER_ROW) * step,
                     y + (i / SWATCHES_PER_ROW) * step, cell, cell);
        item->color = colors[i];
        item->selected = colors[i] == selected;
        item->event.kind = UI_EVENT_SET_STROKE_COLOR;
        item->event.color = colors[i];
    }
    return y + ((count + SWATCHES_PER_ROW - 1) / SWATCHES_PER_ROW) * step - ui_px(scale, SWATCH_GAP);
}

/**
 * @brief Adds a row of three icon buttons that choose one value.
 * @param layout Layout to extend.
 * @param scale Scale factor.
 * @param y Top of the row.
 * @param icons Icons of the buttons.
 * @param count Number of buttons.
 * @param selected Index of the selected button.
 * @param kind Event kind emitted when a button is chosen.
 */
static void choices_add(UiLayout *layout, float scale, int y, const Icon *icons, int count, int selected,
                        UiEventKind kind)
{
    int size = ui_px(scale, SMALL_BUTTON);
    for (int i = 0; i < count; i++) {
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
    static const Icon widthIcons[4] = {ICON_WIDTH_THIN, ICON_WIDTH_MEDIUM, ICON_WIDTH_BOLD, ICON_PRESSURE};
    static const Icon fontIcons[TEXT_FACE_COUNT] = {ICON_FONT_HAND, ICON_FONT_NORMAL, ICON_FONT_LEGIBLE};
    static const Icon styleIcons[3] = {ICON_STYLE_SOLID, ICON_STYLE_DASHED, ICON_STYLE_DOTTED};
    static const Icon laserIcons[2] = {ICON_LASER_TRAIL, ICON_LASER_POINTER};
    static const Icon arrowIcons[ARROW_HEAD_COUNT] = {ICON_ARROW_END, ICON_ARROW_BOTH};
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
        swatches_add(layout, scale, y, STROKE_PALETTE, STROKE_PALETTE_SIZE, style_stroke_index(style->stroke),
                     UI_EVENT_SET_STROKE);
        int after = pad + STROKE_PALETTE_SIZE * (cell + ui_px(scale, SWATCH_GAP));
        int width = ui_px(scale, 1) > 0 ? ui_px(scale, 1) : 1;
        item_add(layout, ITEM_DIVIDER, after, y + (cell - ui_px(scale, 20)) / 2, width, ui_px(scale, 20));
        UiItem *picker =
            item_add(layout, ITEM_PICKER, after + width + ui_px(scale, SWATCH_GAP), y, cell, cell);
        picker->color = style->stroke;
        picker->selected = (model->dialogs & UI_DIALOG_BIT(PANEL_COLOR)) != 0;
        picker->event.kind = UI_EVENT_OPEN_PICKER;
        y += cell;
        if (model->pastel) {
            y = color_rows_add(layout, scale, y + ui_px(scale, SWATCH_GAP), PASTEL_PALETTE,
                               PASTEL_PALETTE_SIZE, style->stroke);
        }
        if (model->customCount > 0) {
            y = color_rows_add(layout, scale, y + ui_px(scale, SWATCH_GAP), model->custom, model->customCount,
                               style->stroke);
        }
        y += gap;
    }
    if (model->sections & UI_SECTION_FILL) {
        y = title_add(layout, scale, y, L"Background");
        swatches_add(layout, scale, y, FILL_PALETTE, FILL_PALETTE_SIZE, style_fill_index(style->fill),
                     UI_EVENT_SET_FILL);
        y += cell + gap;
    }
    if (model->sections & UI_SECTION_FILL_STYLE) {
        static const Icon fillIcons[FILL_STYLE_COUNT] = {ICON_FILL_SOLID, ICON_FILL_HACHURE,
                                                         ICON_FILL_CROSS_HATCH};
        y = title_add(layout, scale, y, L"Fill style");
        choices_add(layout, scale, y, fillIcons, FILL_STYLE_COUNT, (int)style->fillStyle,
                    UI_EVENT_SET_FILL_STYLE);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_WIDTH) {
        y = title_add(layout, scale, y, L"Stroke width");
        bool variable = (model->sections & UI_SECTION_PRESSURE) != 0;
        int selected = variable && style->pressure ? WIDTH_PRESET_COUNT : style_width_index(style);
        choices_add(layout, scale, y, widthIcons, variable ? WIDTH_PRESET_COUNT + 1 : WIDTH_PRESET_COUNT,
                    selected, UI_EVENT_SET_WIDTH);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_FONT) {
        static const wchar_t *const FONT_NAMES[TEXT_FACE_COUNT] = {L"Handwritten", L"Normal", L"Mono"};
        wchar_t title[UI_ITEM_TEXT_CHARS];
        swprintf(title, UI_ITEM_TEXT_CHARS, L"Font - %ls", FONT_NAMES[style->face]);
        y = title_add(layout, scale, y, title);
        choices_add(layout, scale, y, fontIcons, TEXT_FACE_COUNT, (int)style->face, UI_EVENT_SET_FONT);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_FONT_SIZE) {
        static const wchar_t *const SIZE_LABELS[FONT_SIZE_COUNT] = {L"S", L"M", L"L", L"XL"};
        y = title_add(layout, scale, y, L"Font size");
        int size = ui_px(scale, SMALL_BUTTON);
        for (int i = 0; i < FONT_SIZE_COUNT; i++) {
            UiItem *item = item_add(layout, ITEM_TEXT_BUTTON,
                                    pad + i * (size + ui_px(scale, SMALL_BUTTON_GAP)), y, size, size);
            wcscpy(item->text, SIZE_LABELS[i]);
            item->selected = i == style_font_size_index(style);
            item->event.kind = UI_EVENT_SET_FONT_SIZE;
            item->event.index = i;
        }
        y += size + gap;
    }
    if (model->sections & UI_SECTION_LASER) {
        y = title_add(layout, scale, y, L"Laser");
        choices_add(layout, scale, y, laserIcons, 2, model->laserPointer ? 1 : 0, UI_EVENT_SET_LASER_MODE);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_STROKE_STYLE) {
        y = title_add(layout, scale, y, L"Stroke style");
        choices_add(layout, scale, y, styleIcons, 3, (int)style->strokeStyle, UI_EVENT_SET_STROKE_STYLE);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_ARROWHEADS) {
        y = title_add(layout, scale, y, L"Arrowheads");
        choices_add(layout, scale, y, arrowIcons, ARROW_HEAD_COUNT, (int)style->arrowHeads,
                    UI_EVENT_SET_ARROWHEADS);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_CORNERS) {
        static const Icon cornerIcons[CORNERS_COUNT] = {ICON_CORNER_ROUND, ICON_CORNER_SHARP};
        y = title_add(layout, scale, y, L"Corners");
        choices_add(layout, scale, y, cornerIcons, CORNERS_COUNT, (int)style->corners, UI_EVENT_SET_CORNERS);
        y += ui_px(scale, SMALL_BUTTON) + gap;
    }
    if (model->sections & UI_SECTION_OPACITY) {
        float opacity = roundf(fminf(fmaxf(style->opacity, 0.0f), 1.0f) * UI_SLIDER_STEPS) / UI_SLIDER_STEPS;
        UiItem *label = layout->count < UI_MAX_ITEMS ? &layout->items[layout->count] : NULL;
        y = title_add(layout, scale, y, L"Opacity");
        if (label) {
            int valueWidth = ui_px(scale, OPACITY_VALUE_WIDTH);
            int valueLeft = label->rect.right - valueWidth;
            label->rect.right = valueLeft;
            UiItem *value = item_add(layout, ITEM_LABEL, valueLeft, label->rect.top, valueWidth,
                                     ui_px(scale, TITLE_HEIGHT));
            value->alignRight = true;
            value->color = COLOR_ICON;
            swprintf(value->text, UI_ITEM_TEXT_CHARS, L"%d", (int)lroundf(opacity * 100.0f));
        }
        UiItem *slider =
            item_add(layout, ITEM_SLIDER, pad, y, ui_px(scale, SECTION_INNER), ui_px(scale, SLIDER_HEIGHT));
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
    } else if (panel == PANEL_SETTINGS) {
        build_settings(layout, model, scale);
    } else if (panel == PANEL_CONFIRM) {
        build_confirm_clear(layout, scale);
    } else if (panel == PANEL_COLOR) {
        build_color_picker(layout, model, scale);
    } else {
        build_properties(layout, model, pinned, scale);
    }
}

bool ui_item_equal(const UiItem *first, const UiItem *second)
{
    return first->kind == second->kind && memcmp(&first->rect, &second->rect, sizeof first->rect) == 0 &&
           first->icon == second->icon && first->hint == second->hint && first->color == second->color &&
           first->selected == second->selected && first->enabled == second->enabled &&
           first->checker == second->checker && first->alignRight == second->alignRight &&
           first->value == second->value && first->value2 == second->value2 && first->hue == second->hue &&
           wcscmp(first->text, second->text) == 0 && first->event.kind == second->event.kind &&
           first->event.action == second->event.action && first->event.tool == second->event.tool &&
           first->event.index == second->event.index && first->event.value == second->event.value &&
           first->event.color == second->event.color;
}

bool ui_item_draggable(const UiItem *item)
{
    return item->kind == ITEM_SLIDER || item->kind == ITEM_SV || item->kind == ITEM_HUE;
}

void ui_item_drag_values(const UiItem *item, int x, int y, float scale, float *first, float *second)
{
    *second = 0.0f;
    if (item->kind == ITEM_SLIDER) {
        *first = ui_slider_value_at(item, x, scale);
        return;
    }
    int margin = ui_px(scale, UI_PICKER_MARGIN);
    float width = (float)(item->rect.right - item->rect.left - 2 * margin);
    float height = (float)(item->rect.bottom - item->rect.top - 2 * margin);
    *first = fminf(fmaxf((float)(x - item->rect.left - margin) / width, 0.0f), 1.0f);
    if (item->kind == ITEM_SV) {
        *second = 1.0f - fminf(fmaxf((float)(y - item->rect.top - margin) / height, 0.0f), 1.0f);
    }
}

int ui_layout_hit(const UiLayout *layout, int x, int y)
{
    for (int i = layout->count - 1; i >= 0; i--) {
        const UiItem *item = &layout->items[i];
        bool interactive = item->kind == ITEM_BUTTON || item->kind == ITEM_SWATCH ||
                           item->kind == ITEM_SWITCH || item->kind == ITEM_SLIDER ||
                           item->kind == ITEM_TEXT_BUTTON || item->kind == ITEM_PICKER ||
                           item->kind == ITEM_SV || item->kind == ITEM_HUE || item->kind == ITEM_HEX;
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
