/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <windowsx.h>

#include "shapes_ui.h"
#include "ui_internal.h"

#define ISLAND_CLASS L"PenPaneIsland"
#define AUTO_HIDE_TIMER_ID 1
#define AUTO_HIDE_DEFAULT_MS 1200
#define AUTO_HIDE_MIN_MS 200
#define DRAG_THRESHOLD 4
#define EXCLUDE_FROM_CAPTURE 0x11
#define HANDLE_RADIUS 10
#define DEFAULT_EDGE_MARGIN 8
#define DEFAULT_SIDE_MARGIN 16

/** @brief One floating window with its layout, canvas and interaction state. */
typedef struct Island {
    struct Ui *ui;
    PanelId id;
    HWND window;
    Canvas canvas;
    bool hasCanvas;
    UiLayout layout;
    float scale;
    int margin;
    bool shown;
    bool collapsed;
    int x;
    int y;
    int hover;
    int pressed;
    bool handleHover;
    bool tracking;
    bool timerActive;
    bool awaitFirstHover;
    bool savedCollapsed;
    bool hasSaved;
    bool inEyeMode;
    bool pointerDown;
    bool dragging;
    bool dragCandidate;
    bool dragMoved;
    bool sliderActive;
    float sliderLast;
    float sliderLast2;
    bool hexEditing;
    int hexLength;
    wchar_t hexText[8];
    POINT dragCursor;
    POINT dragOrigin;
    unsigned handleSignature;
} Island;

/** @brief UI instance state. */
struct Ui {
    HINSTANCE instance;
    UiEventHandler handler;
    void *user;
    UiSettings settings;
    UiModel model;
    bool visible;
    bool drawing;
    Island islands[PANEL_ISLAND_COUNT];
};

static LRESULT CALLBACK island_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
static void island_set_focusable(Island *island, bool focusable);

/**
 * @brief Reads the display scale of an island window; the single place where DPI is queried.
 * @param island Island to update.
 */
static void island_read_scale(Island *island)
{
    UINT dpi = island->window ? GetDpiForWindow(island->window) : 96;
    island->scale = dpi > 0 ? (float)dpi / 96.0f : 1.0f;
    island->margin = ui_px(island->scale, UI_SHADOW_MARGIN);
}

/**
 * @brief Returns the current content size, either the expanded layout or the collapsed handle.
 * @param island Island to query.
 * @param width Receives the width.
 * @param height Receives the height.
 */
static void island_content_size(const Island *island, int *width, int *height)
{
    if (island->collapsed) {
        *width = *height = ui_px(island->scale, UI_HANDLE_SIZE);
    } else {
        *width = island->layout.width;
        *height = island->layout.height;
    }
}

/**
 * @brief Returns the screen rectangle of the island content, excluding the shadow margin.
 * @param island Island to query.
 * @return Screen rectangle.
 */
static RECT island_screen_rect(const Island *island)
{
    int width;
    int height;
    island_content_size(island, &width, &height);
    RECT rect = {island->x, island->y, island->x + width, island->y + height};
    return rect;
}

/**
 * @brief Tells whether the mouse cursor is currently over the island content.
 * @param island Island to query.
 * @return true when the cursor is inside.
 */
static bool island_cursor_inside(const Island *island)
{
    POINT cursor;
    RECT rect = island_screen_rect(island);
    return GetCursorPos(&cursor) && PtInRect(&rect, cursor);
}

/**
 * @brief Tells whether an island never collapses by itself: pinned, or auto-hide switched off.
 * @param island Island to query.
 * @return true when the island stays expanded without the pointer.
 */
static bool island_keeps_open(const Island *island)
{
    const UiSettings *settings = &island->ui->settings;
    return UI_PANEL_IS_DIALOG(island->id) || !settings->autoHide || settings->pinned[island->id] ||
           (island->id == PANEL_HISTORY && island->ui->model.drawingsHidden);
}

/**
 * @brief Centers the dialog on the monitor that hosts the toolbar.
 * @param island The dialog island, its layout already built.
 */
static void island_center_dialog(Island *island)
{
    const Island *anchor = &island->ui->islands[PANEL_TOOLBAR];
    RECT reference = {anchor->x, anchor->y, anchor->x + 1, anchor->y + 1};
    MONITORINFO info;
    info.cbSize = sizeof info;
    HMONITOR monitor = MonitorFromRect(&reference, MONITOR_DEFAULTTONEAREST);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) {
        return;
    }
    int cascade = ui_px(island->scale, 28) * (int)(island->id - PANEL_SETTINGS);
    island->x =
        info.rcWork.left + (info.rcWork.right - info.rcWork.left - island->layout.width) / 2 + cascade;
    island->y =
        info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - island->layout.height) / 2 + cascade;
    if (island->shown) {
        SetWindowPos(island->window, NULL, island->x - island->margin, island->y - island->margin, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

/**
 * @brief Builds the layout of an island from the model, its pin state and its scale.
 * @param island Island to lay out.
 * @param layout Receives the layout.
 */
static void island_build_layout(const Island *island, UiLayout *layout)
{
    ui_layout_build(island->id, &island->ui->model, island->ui->settings.pinned[island->id], island->scale,
                    layout);
    if (island->hexEditing) {
        for (int i = 0; i < layout->count; i++) {
            if (layout->items[i].kind == ITEM_HEX) {
                swprintf(layout->items[i].text, UI_ITEM_TEXT_CHARS, L"#%ls|", island->hexText);
                layout->items[i].selected = true;
            }
        }
    }
}

/**
 * @brief Delivers an event to the application handler.
 * @param ui UI instance.
 * @param event Event to deliver.
 */
static void emit(Ui *ui, const UiEvent *event)
{
    if (ui->handler) {
        ui->handler(ui->user, event);
    }
}

/**
 * @brief Tells the handler that a slider drag has ended.
 * @param ui UI instance.
 */
static void emit_slider_end(Ui *ui)
{
    UiEvent event;
    memset(&event, 0, sizeof event);
    event.kind = UI_EVENT_SLIDER_END;
    emit(ui, &event);
}

/**
 * @brief Emits a panel moved event and records the position as the saved placement.
 * @param island Island that moved.
 */
static void emit_moved(Island *island)
{
    UiEvent event;
    memset(&event, 0, sizeof event);
    event.kind = UI_EVENT_PANEL_MOVED;
    event.panel = island->id;
    event.x = island->x;
    event.y = island->y;
    island->ui->settings.placement[island->id].set = true;
    island->ui->settings.placement[island->id].x = island->x;
    island->ui->settings.placement[island->id].y = island->y;
    emit(island->ui, &event);
}

/**
 * @brief Keeps at least a strip of the island on the monitor nearest to it.
 * @param island Island whose scale defines the strip size.
 * @param x In/out left edge of the content.
 * @param y In/out top edge of the content.
 * @param width Content width.
 * @param height Content height.
 */
static void island_clamp(const Island *island, int *x, int *y, int width, int height)
{
    RECT rect = {*x, *y, *x + width, *y + height};
    MONITORINFO info;
    info.cbSize = sizeof info;
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) {
        return;
    }
    int strip = ui_px(island->scale, UI_DRAG_STRIP);
    if (*x + width < info.rcMonitor.left + strip) {
        *x = info.rcMonitor.left + strip - width;
    }
    if (*x > info.rcMonitor.right - strip) {
        *x = info.rcMonitor.right - strip;
    }
    if (*y < info.rcMonitor.top) {
        *y = info.rcMonitor.top;
    }
    if (*y > info.rcMonitor.bottom - strip) {
        *y = info.rcMonitor.bottom - strip;
    }
}

/**
 * @brief Moves the window so its content sits at the island position.
 * @param island Island to move.
 */
static void island_move_window(const Island *island)
{
    SetWindowPos(island->window, NULL, island->x - island->margin, island->y - island->margin, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/**
 * @brief Arms or cancels the one-shot auto-hide timer according to the current state.
 * @param island Island to update.
 */
static void island_timer_sync(Island *island)
{
    if (island->timerActive) {
        KillTimer(island->window, AUTO_HIDE_TIMER_ID);
        island->timerActive = false;
    }
    const UiSettings *settings = &island->ui->settings;
    if (island->shown && !island->collapsed && !island->awaitFirstHover && !island_keeps_open(island) &&
        !island->pointerDown && !island_cursor_inside(island)) {
        UINT delay = settings->autoHideMs > 0 ? (UINT)settings->autoHideMs : AUTO_HIDE_DEFAULT_MS;
        if (delay < AUTO_HIDE_MIN_MS) {
            delay = AUTO_HIDE_MIN_MS;
        }
        SetTimer(island->window, AUTO_HIDE_TIMER_ID, delay, NULL);
        island->timerActive = true;
    }
}

/**
 * @brief Pushes the canvas to the screen, either entirely or only a dirty rectangle.
 * @param island Island to present.
 * @param dirty Changed region in canvas pixels, or NULL for the whole canvas with position and size.
 */
static void island_present(Island *island, const RECT *dirty)
{
    if (!island->hasCanvas) {
        return;
    }
    POINT source = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    POINT position = {island->x - island->margin, island->y - island->margin};
    SIZE size = {island->canvas.width, island->canvas.height};
    UPDATELAYEREDWINDOWINFO info;
    memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    info.pptDst = &position;
    info.psize = &size;
    info.hdcSrc = island->canvas.dc;
    info.pptSrc = &source;
    info.pblend = &blend;
    info.dwFlags = ULW_ALPHA;
    info.prcDirty = dirty;
    UpdateLayeredWindowIndirect(island->window, &info);
}

/**
 * @brief Returns the paint state of an item.
 * @param island Island owning the item.
 * @param index Item index.
 * @return Hover and pressed flags.
 */
static ItemState island_item_state(const Island *island, int index)
{
    ItemState state;
    state.hover = island->hover == index;
    state.pressed = island->pressed == index && (island->hover == index || island->sliderActive);
    return state;
}

/**
 * @brief Repaints one item and extends a dirty rectangle.
 * @param island Island to paint.
 * @param index Item index.
 * @param dirty In/out dirty rectangle in canvas pixels.
 * @param any In/out flag that tells whether dirty holds a rectangle yet.
 */
static void island_redraw_item(Island *island, int index, RECT *dirty, bool *any)
{
    const UiItem *item = &island->layout.items[index];
    ui_paint_item(&island->canvas, island->margin, item, island_item_state(island, index), island->scale);
    RECT rect = {item->rect.left + island->margin, item->rect.top + island->margin,
                 item->rect.right + island->margin, item->rect.bottom + island->margin};
    if (*any) {
        UnionRect(dirty, dirty, &rect);
    } else {
        *dirty = rect;
        *any = true;
    }
}

/**
 * @brief Repaints the collapsed handle and presents the whole canvas region.
 * @param island Island in collapsed state.
 */
static void island_redraw_handle(Island *island)
{
    int size = ui_px(island->scale, UI_HANDLE_SIZE);
    ui_paint_body(&island->canvas, island->margin, size, size, (float)ui_px(island->scale, HANDLE_RADIUS),
                  island->scale);
    ui_paint_handle(&island->canvas, island->margin, island->id, &island->ui->model, island->handleHover,
                    island->scale);
    island->handleSignature = ui_handle_signature(island->id, &island->ui->model);
    RECT dirty = {island->margin, island->margin, island->margin + size, island->margin + size};
    island_present(island, &dirty);
}

/**
 * @brief Paints the complete island into its canvas.
 * @param island Island to paint; its canvas must exist.
 */
static void island_paint_all(Island *island)
{
    if (island->collapsed) {
        int size = ui_px(island->scale, UI_HANDLE_SIZE);
        ui_paint_body(&island->canvas, island->margin, size, size, (float)ui_px(island->scale, HANDLE_RADIUS),
                      island->scale);
        ui_paint_handle(&island->canvas, island->margin, island->id, &island->ui->model, island->handleHover,
                        island->scale);
        island->handleSignature = ui_handle_signature(island->id, &island->ui->model);
        return;
    }
    ui_paint_body(&island->canvas, island->margin, island->layout.width, island->layout.height,
                  (float)ui_px(island->scale, (int)ISLAND_RADIUS), island->scale);
    for (int i = 0; i < island->layout.count; i++) {
        ui_paint_item(&island->canvas, island->margin, &island->layout.items[i], island_item_state(island, i),
                      island->scale);
    }
}

/**
 * @brief Recreates the canvas for the current content size, repaints and presents everything.
 * @param island Island whose window is shown or about to be shown.
 */
static void island_apply_geometry(Island *island)
{
    int width;
    int height;
    island_content_size(island, &width, &height);
    int canvasWidth = width + 2 * island->margin;
    int canvasHeight = height + 2 * island->margin;
    if (!island->hasCanvas || island->canvas.width != canvasWidth || island->canvas.height != canvasHeight) {
        if (island->hasCanvas) {
            canvas_destroy(&island->canvas);
        }
        island->hasCanvas = canvas_create(&island->canvas, canvasWidth, canvasHeight);
    }
    if (island->hasCanvas) {
        island_paint_all(island);
        island_present(island, NULL);
    }
}

/**
 * @brief Rebuilds the layout from the model using the current scale.
 * @param island Island to update.
 */
static void island_rebuild_layout(Island *island)
{
    island_build_layout(island, &island->layout);
    island->hover = -1;
    island->pressed = -1;
    island->sliderActive = false;
}

/**
 * @brief Shows the island window with a freshly allocated canvas.
 * @param island Island to show.
 */
static void island_show(Island *island)
{
    island_read_scale(island);
    island_rebuild_layout(island);
    island->collapsed = !island_keeps_open(island) && !island->awaitFirstHover;
    if (island->hasSaved) {
        island->collapsed = island->savedCollapsed && !island_keeps_open(island);
        island->hasSaved = false;
    }
    island->handleHover = false;
    island->tracking = false;
    island->shown = true;
    if (UI_PANEL_IS_DIALOG(island->id)) {
        island_center_dialog(island);
    }
    island_apply_geometry(island);
    if (island->hasCanvas) {
        SetWindowPos(island->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(island->window, SW_SHOWNOACTIVATE);
    }
    island_timer_sync(island);
}

/**
 * @brief Hides the island window and frees its canvas memory.
 * @param island Island to hide.
 */
static void island_hide(Island *island)
{
    if (island->timerActive) {
        KillTimer(island->window, AUTO_HIDE_TIMER_ID);
        island->timerActive = false;
    }
    if (GetCapture() == island->window) {
        ReleaseCapture();
    }
    if (island->hexEditing) {
        island->hexEditing = false;
        island_set_focusable(island, false);
    }
    ShowWindow(island->window, SW_HIDE);
    if (island->hasCanvas) {
        canvas_destroy(&island->canvas);
        island->hasCanvas = false;
    }
    island->shown = false;
    island->pointerDown = false;
    island->dragging = false;
    island->dragCandidate = false;
    island->hover = -1;
    island->pressed = -1;
    island->sliderActive = false;
}

/**
 * @brief Finds the item that reacts to hover at a point, including passive grips.
 * @param island Island to query.
 * @param x Client x relative to the content.
 * @param y Client y relative to the content.
 * @return Item index or -1.
 */
static int island_hover_target(const Island *island, int x, int y)
{
    int hit = ui_layout_hit(&island->layout, x, y);
    if (hit >= 0) {
        return island->layout.items[hit].enabled ? hit : -1;
    }
    for (int i = 0; i < island->layout.count; i++) {
        const UiItem *item = &island->layout.items[i];
        if (item->kind == ITEM_GRIP && x >= item->rect.left && x < item->rect.right && y >= item->rect.top &&
            y < item->rect.bottom) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Finds the item under the mouse cursor, using the expanded layout.
 * @param island Island to query.
 * @return Item index, or -1 when the cursor is outside the expanded island.
 */
static int island_cursor_target(const Island *island)
{
    POINT cursor;
    RECT rect = {island->x, island->y, island->x + island->layout.width, island->y + island->layout.height};
    if (!GetCursorPos(&cursor) || !PtInRect(&rect, cursor)) {
        return -1;
    }
    return island_hover_target(island, cursor.x - rect.left, cursor.y - rect.top);
}

/**
 * @brief Expands a collapsed island, shifting it so that it fits on its monitor.
 * @param island Island to expand.
 */
static void island_expand(Island *island)
{
    if (!island->collapsed) {
        return;
    }
    RECT rect = {island->x, island->y, island->x + island->layout.width, island->y + island->layout.height};
    MONITORINFO info;
    info.cbSize = sizeof info;
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        if (island->x + island->layout.width > info.rcWork.right) {
            island->x = info.rcWork.right - island->layout.width;
        }
        if (island->x < info.rcWork.left) {
            island->x = info.rcWork.left;
        }
        if (island->y + island->layout.height > info.rcWork.bottom) {
            island->y = info.rcWork.bottom - island->layout.height;
        }
        if (island->y < info.rcWork.top) {
            island->y = info.rcWork.top;
        }
    }
    island->collapsed = false;
    island->handleHover = false;
    island->tracking = false;
    island->pressed = -1;
    island->hover = island_cursor_target(island);
    island_apply_geometry(island);
    island_timer_sync(island);
}

/**
 * @brief Collapses an expanded island to its handle.
 * @param island Island to collapse.
 */
static void island_collapse(Island *island)
{
    if (island->collapsed) {
        return;
    }
    island->collapsed = true;
    island->tracking = false;
    island->handleHover = island_cursor_inside(island);
    island->hover = -1;
    island->pressed = -1;
    island_apply_geometry(island);
    island_timer_sync(island);
}

/**
 * @brief Changes which item is hovered and repaints the two affected items.
 * @param island Island to update.
 * @param hover New hovered item index, or -1.
 */
static void island_set_hover(Island *island, int hover)
{
    if (island->hover == hover || !island->hasCanvas || island->collapsed) {
        island->hover = island->collapsed ? -1 : island->hover;
        return;
    }
    int previous = island->hover;
    island->hover = hover;
    RECT dirty = {0, 0, 0, 0};
    bool any = false;
    if (previous >= 0 && previous < island->layout.count) {
        island_redraw_item(island, previous, &dirty, &any);
    }
    if (hover >= 0 && hover < island->layout.count) {
        island_redraw_item(island, hover, &dirty, &any);
    }
    if (any) {
        island_present(island, &dirty);
    }
}

/**
 * @brief Keeps the toolbar centered on the same point when tools are shown or hidden and its width changes.
 * @param island Toolbar island that is open; its new width is already stored in island->layout.
 * @param previousWidth Width of the toolbar before the change.
 */
static void island_keep_center(Island *island, int previousWidth)
{
    island->x += (previousWidth - island->layout.width) / 2;
    if (island->ui->settings.placement[island->id].set) {
        emit_moved(island);
    }
}

/**
 * @brief Applies the model to one island, repainting only the items that changed.
 * @param island Island to update.
 */
static void island_update(Island *island)
{
    float previousScale = island->scale;
    int previousWidth = island->layout.width;
    island_read_scale(island);
    UiLayout next;
    island_build_layout(island, &next);
    bool sameGeometry = previousScale == island->scale && next.count == island->layout.count &&
                        next.width == island->layout.width && next.height == island->layout.height;
    if (!sameGeometry) {
        bool sizeChanged = previousScale != island->scale || next.width != island->layout.width ||
                           next.height != island->layout.height;
        island->layout = next;
        island->hover = -1;
        island->pressed = -1;
        island->sliderActive = false;
        if (island->shown) {
            if (UI_PANEL_IS_DIALOG(island->id) && sizeChanged) {
                island_center_dialog(island);
            } else if (island->id == PANEL_TOOLBAR && !island->collapsed && previousScale == island->scale) {
                island_keep_center(island, previousWidth);
            }
            island_apply_geometry(island);
        }
        return;
    }
    if (!island->shown || !island->hasCanvas) {
        island->layout = next;
        return;
    }
    if (island->collapsed) {
        island->layout = next;
        if (island->handleSignature != ui_handle_signature(island->id, &island->ui->model)) {
            island_redraw_handle(island);
        }
        return;
    }
    RECT dirty = {0, 0, 0, 0};
    bool any = false;
    for (int i = 0; i < next.count; i++) {
        if (!ui_item_equal(&island->layout.items[i], &next.items[i])) {
            island->layout.items[i] = next.items[i];
            island_redraw_item(island, i, &dirty, &any);
        }
    }
    if (any) {
        island_present(island, &dirty);
    }
}

/**
 * @brief Shows, hides or re-modes an island according to the global visibility state.
 * @param island Island to refresh.
 */
static void island_refresh_visibility(Island *island)
{
    const Ui *ui = island->ui;
    bool want = ui->visible && (ui->drawing || island->id == PANEL_TOOLBAR) &&
                (island->id != PANEL_PROPERTIES || ui->model.sections != 0);
    if (UI_PANEL_IS_DIALOG(island->id)) {
        want = ui->visible && (ui->model.dialogs & UI_DIALOG_BIT(island->id)) != 0;
    }
    bool eyeMode = ui->visible && ui->model.drawingsHidden && !UI_PANEL_IS_DIALOG(island->id);
    if (eyeMode) {
        want = island->id == PANEL_HISTORY;
    }
    if (!want) {
        if (island->shown) {
            island->savedCollapsed = island->collapsed;
            island->hasSaved = eyeMode;
            island_hide(island);
        }
        return;
    }
    if (!island->shown) {
        island_show(island);
        island->inEyeMode = eyeMode;
        return;
    }
    if (island->id == PANEL_HISTORY && eyeMode != island->inEyeMode) {
        island->inEyeMode = eyeMode;
        if (eyeMode) {
            island->savedCollapsed = island->collapsed;
            island_expand(island);
        } else if (island->savedCollapsed && !island_keeps_open(island)) {
            island_collapse(island);
        }
    }
}

/**
 * @brief Converts typed hex digits to a color; three digits expand like in CSS.
 * @param digits Typed digits.
 * @param length Number of digits.
 * @param color Receives the opaque color.
 * @return true when the digits form a color.
 */
static bool hex_color(const wchar_t *digits, int length, Color *color)
{
    char text[7];
    if (length == 6) {
        for (int i = 0; i < 6; i++) {
            text[i] = (char)digits[i];
        }
    } else if (length == 3) {
        for (int i = 0; i < 3; i++) {
            text[2 * i] = text[2 * i + 1] = (char)digits[i];
        }
    } else {
        return false;
    }
    text[6] = '\0';
    return color_parse_hex(text, color);
}

/**
 * @brief Lets a dialog take the keyboard focus or gives it up again.
 * @param island Dialog island.
 * @param focusable true to allow activation and focus the window.
 */
static void island_set_focusable(Island *island, bool focusable)
{
    LONG_PTR style = GetWindowLongPtrW(island->window, GWL_EXSTYLE);
    style = focusable ? (style & ~(LONG_PTR)WS_EX_NOACTIVATE) : (style | WS_EX_NOACTIVATE);
    SetWindowLongPtrW(island->window, GWL_EXSTYLE, style);
    if (focusable) {
        SetForegroundWindow(island->window);
        SetFocus(island->window);
    }
}

/**
 * @brief Starts typing a hex color into the field of the color dialog.
 * @param island Color dialog island.
 */
static void island_hex_begin(Island *island)
{
    if (island->hexEditing) {
        return;
    }
    island->hexEditing = true;
    island->hexLength = 0;
    island->hexText[0] = L'\0';
    island_set_focusable(island, true);
    island_update(island);
}

/**
 * @brief Ends typing; a complete color is reported as a stroke color first.
 * @param island Color dialog island.
 * @param commit true to apply the typed digits when they form a color.
 */
static void island_hex_end(Island *island, bool commit)
{
    if (!island->hexEditing) {
        return;
    }
    island->hexEditing = false;
    island_set_focusable(island, false);
    Color color;
    UiEvent event;
    memset(&event, 0, sizeof event);
    if (commit && hex_color(island->hexText, island->hexLength, &color)) {
        event.kind = UI_EVENT_SET_STROKE_COLOR;
        event.color = color;
        emit(island->ui, &event);
    }
    island_update(island);
    event.kind = UI_EVENT_EDIT_END;
    emit(island->ui, &event);
}

/**
 * @brief Appends the hex digits found in the clipboard text to the typed value.
 * @param island Color dialog island.
 */
static void island_hex_paste(Island *island)
{
    if (!OpenClipboard(island->window)) {
        return;
    }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *text = data ? (const wchar_t *)GlobalLock(data) : NULL;
    if (text) {
        island->hexLength = 0;
        for (const wchar_t *c = text; *c && island->hexLength < 6; c++) {
            if (iswxdigit(*c)) {
                island->hexText[island->hexLength++] = (wchar_t)towupper(*c);
            }
        }
        island->hexText[island->hexLength] = L'\0';
        GlobalUnlock(data);
    }
    CloseClipboard();
}

/**
 * @brief Handles a typed character while a hex color is being entered.
 * @param island Color dialog island.
 * @param c Character from WM_CHAR.
 */
static void island_hex_char(Island *island, wchar_t c)
{
    if (c == L'\r') {
        island_hex_end(island, true);
        return;
    }
    if (c == 27) {
        island_hex_end(island, false);
        return;
    }
    if (c == L'\b') {
        if (island->hexLength > 0) {
            island->hexText[--island->hexLength] = L'\0';
        }
    } else if (c == 0x16) {
        island_hex_paste(island);
    } else if (island->hexLength < 6 && iswxdigit(c)) {
        island->hexText[island->hexLength++] = (wchar_t)towupper(c);
        island->hexText[island->hexLength] = L'\0';
    } else {
        return;
    }
    island_update(island);
}

/**
 * @brief Changes the pin state of an island, repaints its pin button and re-evaluates the timer.
 * @param island Island to change.
 * @param pinned true to keep the island open.
 */
static void island_set_pinned(Island *island, bool pinned)
{
    island->ui->settings.pinned[island->id] = pinned;
    island_update(island);
    island_timer_sync(island);
}

/**
 * @brief Handles a mouse press inside the island.
 * @param island Island receiving the press.
 * @param x Client x.
 * @param y Client y.
 */
static void island_press(Island *island, int x, int y)
{
    int cx = x - island->margin;
    int cy = y - island->margin;
    SetCapture(island->window);
    island->pointerDown = true;
    island->dragMoved = false;
    island->dragging = false;
    island->dragCandidate = false;
    GetCursorPos(&island->dragCursor);
    island->dragOrigin.x = island->x;
    island->dragOrigin.y = island->y;
    if (island->timerActive) {
        KillTimer(island->window, AUTO_HIDE_TIMER_ID);
        island->timerActive = false;
    }
    if (island->collapsed) {
        island->dragCandidate = true;
        return;
    }
    int hit = ui_layout_hit(&island->layout, cx, cy);
    if (hit < 0) {
        island->dragging = true;
        return;
    }
    const UiItem *item = &island->layout.items[hit];
    if (island->hexEditing && item->kind != ITEM_HEX) {
        island_hex_end(island, true);
        item = &island->layout.items[hit];
    }
    if (!item->enabled) {
        island->pointerDown = false;
        ReleaseCapture();
        return;
    }
    island->pressed = hit;
    RECT dirty = {0, 0, 0, 0};
    bool any = false;
    if (ui_item_draggable(item)) {
        float value;
        float second;
        ui_item_drag_values(item, cx, cy, island->scale, &value, &second);
        UiEvent event = item->event;
        island->sliderActive = true;
        island->sliderLast = value;
        island->sliderLast2 = second;
        island->layout.items[hit].value = value;
        island->layout.items[hit].value2 = second;
        island->layout.items[hit].event.value = value;
        island_redraw_item(island, hit, &dirty, &any);
        island_present(island, &dirty);
        event.value = value;
        event.extra = second;
        emit(island->ui, &event);
        return;
    }
    island_redraw_item(island, hit, &dirty, &any);
    island_present(island, &dirty);
}

/**
 * @brief Handles pointer movement while a button is held.
 * @param island Island holding the capture.
 * @param x Client x.
 * @param y Client y.
 */
static void island_drag(Island *island, int x, int y)
{
    int cx = x - island->margin;
    if (island->dragCandidate || island->dragging) {
        POINT cursor;
        GetCursorPos(&cursor);
        int dx = cursor.x - island->dragCursor.x;
        int dy = cursor.y - island->dragCursor.y;
        if (island->dragCandidate && (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)) {
            island->dragCandidate = false;
            island->dragging = true;
        }
        if (island->dragging) {
            int width;
            int height;
            island_content_size(island, &width, &height);
            int nx = island->dragOrigin.x + dx;
            int ny = island->dragOrigin.y + dy;
            island_clamp(island, &nx, &ny, width, height);
            if (nx != island->x || ny != island->y) {
                island->x = nx;
                island->y = ny;
                island->dragMoved = true;
                island_move_window(island);
            }
        }
        return;
    }
    if (island->pressed < 0) {
        return;
    }
    UiItem *item = &island->layout.items[island->pressed];
    if (island->sliderActive) {
        float value;
        float second;
        ui_item_drag_values(item, cx, y - island->margin, island->scale, &value, &second);
        if (value != island->sliderLast || second != island->sliderLast2) {
            UiEvent event = item->event;
            island->sliderLast = value;
            island->sliderLast2 = second;
            item->value = value;
            item->value2 = second;
            item->event.value = value;
            RECT dirty = {0, 0, 0, 0};
            bool any = false;
            island_redraw_item(island, island->pressed, &dirty, &any);
            island_present(island, &dirty);
            event.value = value;
            event.extra = second;
            emit(island->ui, &event);
        }
        return;
    }
    int hover = island_hover_target(island, cx, y - island->margin);
    island_set_hover(island, hover);
}

/**
 * @brief Finishes a press: reports moves or clicks and restores idle state.
 * @param island Island that held the capture.
 * @param x Client x.
 * @param y Client y.
 */
static void island_release(Island *island, int x, int y)
{
    if (!island->pointerDown) {
        return;
    }
    int cx = x - island->margin;
    int cy = y - island->margin;
    bool moved = island->dragging && island->dragMoved;
    bool handleClick = island->dragCandidate && island->collapsed && island_cursor_inside(island);
    int pressed = island->pressed;
    bool sliderEnded = island->sliderActive;
    UiEvent event;
    memset(&event, 0, sizeof event);
    bool fire = false;
    if (pressed >= 0 && !island->sliderActive && ui_layout_hit(&island->layout, cx, cy) == pressed) {
        event = island->layout.items[pressed].event;
        fire = true;
    }
    island->pointerDown = false;
    island->dragging = false;
    island->dragCandidate = false;
    island->sliderActive = false;
    island->pressed = -1;
    ReleaseCapture();
    if (island->hasCanvas && !island->collapsed && pressed >= 0 && pressed < island->layout.count) {
        RECT dirty = {0, 0, 0, 0};
        bool any = false;
        island_redraw_item(island, pressed, &dirty, &any);
        island_present(island, &dirty);
    }
    if (!island_cursor_inside(island)) {
        island_set_hover(island, -1);
        if (island->collapsed && island->handleHover) {
            island->handleHover = false;
            island_redraw_handle(island);
        }
    }
    island_timer_sync(island);
    if (sliderEnded) {
        emit_slider_end(island->ui);
    }
    if (moved) {
        emit_moved(island);
    } else if (handleClick) {
        island_expand(island);
    } else if (fire) {
        if (event.kind == UI_EVENT_EDIT_HEX) {
            island_hex_begin(island);
            return;
        }
        if (event.kind == UI_EVENT_PANEL_PINNED) {
            island_set_pinned(island, event.index != 0);
        }
        emit(island->ui, &event);
    }
}

/**
 * @brief Handles plain pointer movement without a pressed button.
 * @param island Island under the pointer.
 * @param x Client x.
 * @param y Client y.
 */
static void island_hover_move(Island *island, int x, int y)
{
    island->awaitFirstHover = false;
    if (!island->tracking) {
        TRACKMOUSEEVENT track = {sizeof track, TME_LEAVE, island->window, 0};
        if (TrackMouseEvent(&track)) {
            island->tracking = true;
        }
    }
    if (island->timerActive) {
        KillTimer(island->window, AUTO_HIDE_TIMER_ID);
        island->timerActive = false;
    }
    if (island->collapsed) {
        if (!island->handleHover && island->hasCanvas) {
            island->handleHover = true;
            island_redraw_handle(island);
        }
        return;
    }
    island_set_hover(island, island_hover_target(island, x - island->margin, y - island->margin));
}

/**
 * @brief Handles the pointer leaving the window.
 * @param island Island the pointer left.
 */
static void island_leave(Island *island)
{
    island->tracking = false;
    if (island->pointerDown) {
        return;
    }
    if (island->collapsed) {
        if (island->handleHover && island->hasCanvas) {
            island->handleHover = false;
            island_redraw_handle(island);
        }
        return;
    }
    island_set_hover(island, -1);
    island_timer_sync(island);
}

/**
 * @brief Window procedure of all island windows.
 * @param window Window handle.
 * @param message Window message.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return Message result.
 */
static LRESULT CALLBACK island_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return DefWindowProcW(window, message, wparam, lparam);
    }
    Island *island = (Island *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (!island) {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        RECT rect = island_screen_rect(island);
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        return PtInRect(&rect, point) ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        if (island->pointerDown) {
            island_drag(island, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } else {
            island_hover_move(island, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        }
        return 0;
    case WM_MOUSELEAVE:
        island_leave(island);
        return 0;
    case WM_LBUTTONDOWN:
        island_press(island, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;
    case WM_LBUTTONUP:
        island_release(island, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;
    case WM_CHAR:
        if (island->hexEditing) {
            island_hex_char(island, (wchar_t)wparam);
        }
        return 0;
    case WM_KILLFOCUS:
        island_hex_end(island, true);
        return 0;
    case WM_CAPTURECHANGED:
        if (island->pointerDown) {
            bool sliderEnded = island->sliderActive;
            island->pointerDown = false;
            island->dragging = false;
            island->dragCandidate = false;
            island->sliderActive = false;
            island->pressed = -1;
            if (sliderEnded) {
                emit_slider_end(island->ui);
            }
        }
        return 0;
    case WM_TIMER:
        if (wparam == AUTO_HIDE_TIMER_ID) {
            KillTimer(window, AUTO_HIDE_TIMER_ID);
            island->timerActive = false;
            if (island->shown && !island->pointerDown && !island_keeps_open(island) &&
                !island_cursor_inside(island)) {
                island_collapse(island);
            }
        }
        return 0;
    case WM_DPICHANGED:
        if (island->shown) {
            island_update(island);
        }
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

Ui *ui_create(HINSTANCE instance, UiEventHandler handler, void *user, const UiSettings *settings)
{
    Ui *ui = calloc(1, sizeof *ui);
    if (!ui) {
        return NULL;
    }
    ui->instance = instance;
    ui->handler = handler;
    ui->user = user;
    ui->settings = *settings;
    ui->model.style = style_default();
    ui->model.tool = TOOL_PEN;

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof windowClass;
    windowClass.lpfnWndProc = island_procedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.lpszClassName = ISLAND_CLASS;
    RegisterClassExW(&windowClass);

    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        Island *island = &ui->islands[i];
        island->ui = ui;
        island->id = (PanelId)i;
        island->hover = -1;
        island->pressed = -1;
        island->awaitFirstHover = i == PANEL_TOOLBAR;
        island->window =
            CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ISLAND_CLASS,
                            L"PenPane island", WS_POPUP, 0, 0, 1, 1, NULL, NULL, instance, island);
        if (!island->window) {
            ui_destroy(ui);
            return NULL;
        }
        island_read_scale(island);
        island_build_layout(island, &island->layout);
        if (settings->placement[i].set) {
            island->x = settings->placement[i].x;
            island->y = settings->placement[i].y;
        }
    }
    if (settings->excludeFromCapture) {
        ui_set_capture_excluded(ui, true);
    }
    return ui;
}

void ui_destroy(Ui *ui)
{
    if (!ui) {
        return;
    }
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        Island *island = &ui->islands[i];
        if (island->window) {
            DestroyWindow(island->window);
        }
        if (island->hasCanvas) {
            canvas_destroy(&island->canvas);
        }
    }
    UnregisterClassW(ISLAND_CLASS, ui->instance);
    ui_scratch_release();
    free(ui);
}

void ui_update(Ui *ui, const UiModel *model)
{
    ui->model = *model;
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        island_update(&ui->islands[i]);
    }
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        island_refresh_visibility(&ui->islands[i]);
    }
}

void ui_apply_settings(Ui *ui, const UiSettings *settings)
{
    ui->settings = *settings;
    ui_set_capture_excluded(ui, settings->excludeFromCapture);
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        Island *island = &ui->islands[i];
        island_update(island);
        if (island->shown && island->collapsed && island_keeps_open(island)) {
            island_expand(island);
        }
        island_timer_sync(island);
    }
}

void ui_set_visible(Ui *ui, bool visible, bool drawing)
{
    ui->visible = visible;
    ui->drawing = drawing;
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        island_refresh_visibility(&ui->islands[i]);
    }
}

void ui_place_defaults(Ui *ui, RECT workArea, bool forceAll)
{
    for (int i = 0; i < PANEL_COUNT; i++) {
        Island *island = &ui->islands[i];
        if (!forceAll && ui->settings.placement[i].set) {
            island->x = ui->settings.placement[i].x;
            island->y = ui->settings.placement[i].y;
        } else {
            int edge = ui_px(island->scale, DEFAULT_EDGE_MARGIN);
            int side = ui_px(island->scale, DEFAULT_SIDE_MARGIN);
            if (i == PANEL_TOOLBAR) {
                island->x = workArea.left + (workArea.right - workArea.left - island->layout.width) / 2;
                island->y = workArea.top + edge;
            } else if (i == PANEL_PROPERTIES) {
                island->x = workArea.left + side;
                island->y = workArea.top + edge;
            } else {
                island->x = workArea.left + side;
                island->y = workArea.bottom - side - island->layout.height;
            }
        }
        int width;
        int height;
        island_content_size(island, &width, &height);
        island_clamp(island, &island->x, &island->y, width, height);
        if (island->shown) {
            island_move_window(island);
        }
        if (forceAll) {
            emit_moved(island);
        }
    }
}

bool ui_set_capture_excluded(Ui *ui, bool exclude)
{
    bool ok = true;
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        if (!SetWindowDisplayAffinity(ui->islands[i].window, exclude ? EXCLUDE_FROM_CAPTURE : 0)) {
            ok = false;
        }
    }
    if (ok) {
        ui->settings.excludeFromCapture = exclude;
    }
    return ok;
}

void ui_raise(Ui *ui)
{
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        if (ui->islands[i].shown) {
            SetWindowPos(ui->islands[i].window, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

bool ui_contains_point(const Ui *ui, POINT point)
{
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        if (ui->islands[i].shown) {
            RECT rect = island_screen_rect(&ui->islands[i]);
            if (PtInRect(&rect, point)) {
                return true;
            }
        }
    }
    return false;
}

#ifdef PENPANE_UI_DUMP
bool ui_dump(const Ui *ui, const char *path)
{
    RECT box = {0x7FFFFFFF, 0x7FFFFFFF, -0x7FFFFFFF, -0x7FFFFFFF};
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        const Island *island = &ui->islands[i];
        if (island->shown && island->hasCanvas) {
            RECT rect = {island->x - island->margin, island->y - island->margin,
                         island->x - island->margin + island->canvas.width,
                         island->y - island->margin + island->canvas.height};
            UnionRect(&box, box.left == 0x7FFFFFFF ? &rect : &box, &rect);
        }
    }
    const char *backdropPath = getenv("PENPANE_DUMP_BG");
    FILE *backdrop = backdropPath ? fopen(backdropPath, "rb") : NULL;
    if (backdrop) {
        SetRect(&box, 0, 0, 1920, 1200);
    }
    int width = box.right - box.left;
    int height = box.bottom - box.top;
    if (width <= 0 || height <= 0) {
        if (backdrop) {
            fclose(backdrop);
        }
        return false;
    }
    size_t stride = ((size_t)width * 3 + 3) & ~(size_t)3;
    unsigned char *data = malloc(stride * (size_t)height);
    if (!data) {
        if (backdrop) {
            fclose(backdrop);
        }
        return false;
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char *out = data + (size_t)(height - 1 - y) * stride + (size_t)x * 3;
            unsigned char bgra[4];
            if (backdrop && fread(bgra, 1, 4, backdrop) == 4) {
                out[0] = bgra[0];
                out[1] = bgra[1];
                out[2] = bgra[2];
                continue;
            }
            float t = (float)y / (float)height;
            out[0] = (unsigned char)(120 + 40 * t);
            out[1] = (unsigned char)(40 + 30 * t);
            out[2] = (unsigned char)(10 + 20 * t);
        }
    }
    if (backdrop) {
        fclose(backdrop);
    }
    for (int i = 0; i < PANEL_ISLAND_COUNT; i++) {
        const Island *island = &ui->islands[i];
        if (!island->shown || !island->hasCanvas) {
            continue;
        }
        for (int y = 0; y < island->canvas.height; y++) {
            for (int x = 0; x < island->canvas.width; x++) {
                uint32_t pixel = island->canvas.pixels[(size_t)y * (size_t)island->canvas.width + (size_t)x];
                int dx = island->x - island->margin + x - box.left;
                int dy = island->y - island->margin + y - box.top;
                if (dx < 0 || dy < 0 || dx >= width || dy >= height) {
                    continue;
                }
                unsigned alpha = pixel >> 24;
                unsigned char *out = data + (size_t)(height - 1 - dy) * stride + (size_t)dx * 3;
                out[0] = (unsigned char)((pixel & 0xFFu) + out[0] * (255u - alpha) / 255u);
                out[1] = (unsigned char)(((pixel >> 8) & 0xFFu) + out[1] * (255u - alpha) / 255u);
                out[2] = (unsigned char)(((pixel >> 16) & 0xFFu) + out[2] * (255u - alpha) / 255u);
            }
        }
    }
    unsigned char header[54] = {'B', 'M'};
    uint32_t fileSize = (uint32_t)(54 + stride * (size_t)height);
    memcpy(header + 2, &fileSize, 4);
    header[10] = 54;
    header[14] = 40;
    int32_t w = width;
    int32_t h = height;
    memcpy(header + 18, &w, 4);
    memcpy(header + 22, &h, 4);
    header[26] = 1;
    header[28] = 24;
    FILE *file = fopen(path, "wb");
    bool ok = file && fwrite(header, 1, 54, file) == 54 &&
              fwrite(data, 1, stride * (size_t)height, file) == stride * (size_t)height;
    if (file) {
        fclose(file);
    }
    free(data);
    return ok;
}
#endif

#ifdef PENPANE_UI_DUMP
bool ui_test_drag(Ui *ui, PanelId panel, int kind, const float (*points)[2], int count)
{
    Island *island = &ui->islands[panel];
    const UiItem *item = NULL;
    for (int i = 0; i < island->layout.count && !item; i++) {
        item = (int)island->layout.items[i].kind == kind ? &island->layout.items[i] : NULL;
    }
    if (!island->shown || !item || count < 2) {
        return false;
    }
    RECT rect = item->rect;
    for (int i = 0; i < count; i++) {
        int x = rect.left + (int)((float)(rect.right - rect.left) * points[i][0]) + island->margin;
        int y = rect.top + (int)((float)(rect.bottom - rect.top) * points[i][1]) + island->margin;
        if (i == 0) {
            island_press(island, x, y);
        } else if (i == count - 1) {
            island_release(island, x, y);
        } else {
            island_drag(island, x, y);
        }
    }
    return true;
}
#endif
