/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tray.h"

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "raster.h"
#include "version.h"

#define TRAY_ICON_ID 1
#define TRAY_TOOLTIP L"PenPane " PENPANE_VERSION_TEXT
#define AUTOSTART_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTOSTART_VALUE L"PenPane"
#define ICON_DESIGN_SIZE 32.0f
#define ACTION_NAME_CHARS 48

#define ID_TOGGLE_DRAWING 1
#define ID_MODE_DESKTOP 2
#define ID_MODE_WHITEBOARD 3
#define ID_HIDE_FROM_CAPTURE 4
#define ID_AUTO_HIDE 5
#define ID_CANCEL_LEARN 6
#define ID_COPY_PNG 7
#define ID_RELOAD_CONFIG 8
#define ID_OPEN_CONFIG 9
#define ID_AUTOSTART 10
#define ID_QUIT 11
#define ID_SAVE_PNG 12
#define ID_DRAW_MONITOR_BASE 100
#define ID_UI_MONITOR_BASE 200
#define ID_LEARN_BASE 1000

/** @brief Notification icon instance. */
struct Tray {
    HWND window;
    UINT callbackMessage;
    HICON icon;
    bool registered;
    NOTIFYICONDATAW data;
};

/**
 * @brief Draws one capsule of the pen glyph scaled to the icon size.
 * @param mask Target mask.
 * @param unit Pixels per design unit.
 * @param fromX Start x in design units.
 * @param fromY Start y in design units.
 * @param toX End x in design units.
 * @param toY End y in design units.
 * @param radiusFrom Start radius in design units.
 * @param radiusTo End radius in design units.
 */
static void glyph_capsule(Mask *mask, float unit, float fromX, float fromY, float toX, float toY,
                          float radiusFrom, float radiusTo)
{
    mask_add_capsule(mask, (Point){fromX * unit, fromY * unit}, (Point){toX * unit, toY * unit},
                     radiusFrom * unit, radiusTo * unit);
}

/**
 * @brief Rasterizes the pen glyph into a 32-bit premultiplied canvas.
 * @param canvas Square canvas receiving the glyph.
 */
static void glyph_paint(Canvas *canvas)
{
    float unit = (float)canvas->width / ICON_DESIGN_SIZE;
    Mask mask;
    if (mask_create(&mask, 0, 0, canvas->width, canvas->height)) {
        mask_add_rect_fill(&mask, 0.0f, 0.0f, (float)canvas->width, (float)canvas->height, 7.0f * unit);
        mask_blend(canvas, &mask, COLOR_ISLAND_SELECTED);
        mask_destroy(&mask);
    }
    if (mask_create(&mask, 0, 0, canvas->width, canvas->height)) {
        glyph_capsule(&mask, unit, 21.5f, 9.0f, 12.0f, 18.5f, 3.4f, 3.4f);
        glyph_capsule(&mask, unit, 12.0f, 18.5f, 8.2f, 23.2f, 3.4f, 0.6f);
        glyph_capsule(&mask, unit, 14.0f, 26.0f, 25.0f, 26.0f, 1.3f, 1.3f);
        mask_blend(canvas, &mask, COLOR_ICON_SELECTED);
        mask_destroy(&mask);
    }
}

/**
 * @brief Builds the notification icon at runtime from the rasterized pen glyph.
 * @return Icon handle owned by the caller, or NULL on failure.
 */
static HICON icon_create(void)
{
    int size = GetSystemMetrics(SM_CXSMICON);
    if (size < 16) {
        size = 16;
    }
    Canvas canvas;
    if (!canvas_create(&canvas, size, size)) {
        return NULL;
    }
    glyph_paint(&canvas);
    size_t maskBytes = (size_t)((size + 15) / 16) * 2 * (size_t)size;
    uint8_t *maskBits = calloc(maskBytes, 1);
    HBITMAP mask = maskBits ? CreateBitmap(size, size, 1, 1, maskBits) : NULL;
    HICON icon = NULL;
    if (mask) {
        SelectObject(canvas.dc, canvas.previousBitmap);
        ICONINFO info = {TRUE, 0, 0, mask, canvas.bitmap};
        icon = CreateIconIndirect(&info);
        DeleteObject(mask);
    }
    free(maskBits);
    canvas_destroy(&canvas);
    return icon;
}

/**
 * @brief Registers the icon with the shell and selects the version 4 callback protocol.
 * @param tray Instance whose data is registered.
 * @return true on success.
 */
static bool icon_register(Tray *tray)
{
    bool added = Shell_NotifyIconW(NIM_ADD, &tray->data) != FALSE;
    if (!added) {
        added = Shell_NotifyIconW(NIM_MODIFY, &tray->data) != FALSE;
    }
    if (added) {
        tray->data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray->data);
    }
    tray->registered = added;
    return added;
}

Tray *tray_create(HWND window, UINT callbackMessage)
{
    Tray *tray = calloc(1, sizeof *tray);
    if (!tray) {
        return NULL;
    }
    tray->window = window;
    tray->callbackMessage = callbackMessage;
    tray->icon = icon_create();
    tray->data.cbSize = sizeof tray->data;
    tray->data.hWnd = window;
    tray->data.uID = TRAY_ICON_ID;
    tray->data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray->data.uCallbackMessage = callbackMessage;
    tray->data.hIcon = tray->icon ? tray->icon : LoadIconW(NULL, IDI_APPLICATION);
    wcsncpy(tray->data.szTip, TRAY_TOOLTIP, ARRAYSIZE(tray->data.szTip) - 1);
    icon_register(tray);
    return tray;
}

void tray_destroy(Tray *tray)
{
    if (!tray) {
        return;
    }
    if (tray->registered) {
        Shell_NotifyIconW(NIM_DELETE, &tray->data);
    }
    if (tray->icon) {
        DestroyIcon(tray->icon);
    }
    free(tray);
}

bool tray_restore(Tray *tray)
{
    if (!tray) {
        return false;
    }
    tray->registered = false;
    return icon_register(tray);
}

bool tray_retry(Tray *tray)
{
    if (!tray) {
        return false;
    }
    return tray->registered || icon_register(tray);
}

bool tray_is_registered(const Tray *tray)
{
    return tray != NULL && tray->registered;
}

bool tray_notify(Tray *tray, const wchar_t *title, const wchar_t *text)
{
    if (!tray || !tray->registered) {
        return false;
    }
    NOTIFYICONDATAW data = tray->data;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO;
    wcsncpy(data.szInfoTitle, title, ARRAYSIZE(data.szInfoTitle) - 1);
    wcsncpy(data.szInfo, text, ARRAYSIZE(data.szInfo) - 1);
    return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
}

/**
 * @brief Turns an action id such as "toggle_mode" into the readable "Toggle mode".
 * @param action Action to name.
 * @param buffer Output buffer.
 * @param size Buffer size in characters.
 */
static void action_label(Action action, wchar_t *buffer, int size)
{
    const char *name = action_name(action);
    int i = 0;
    for (; name[i] && i < size - 1; i++) {
        buffer[i] = name[i] == '_' ? L' ' : (wchar_t)(unsigned char)name[i];
    }
    buffer[i] = 0;
    if (buffer[0] >= L'a' && buffer[0] <= L'z') {
        buffer[0] = (wchar_t)(buffer[0] - L'a' + L'A');
    }
}

/**
 * @brief Builds a submenu listing screens as radio items.
 * @param count Number of screens.
 * @param selected Index of the selected screen.
 * @param baseId Command id of the first screen.
 * @return Submenu handle, ownership passes to the parent menu.
 */
static HMENU screens_menu(int count, int selected, int baseId)
{
    HMENU menu = CreatePopupMenu();
    if (count > TRAY_MAX_MONITORS) {
        count = TRAY_MAX_MONITORS;
    }
    for (int i = 0; i < count; i++) {
        wchar_t label[24];
        swprintf(label, ARRAYSIZE(label), L"Screen %d", i + 1);
        AppendMenuW(menu, MF_STRING, (UINT_PTR)(baseId + i), label);
    }
    if (count > 0) {
        CheckMenuRadioItem(menu, (UINT)baseId, (UINT)(baseId + count - 1),
                           (UINT)(baseId + (selected < count ? selected : 0)), MF_BYCOMMAND);
    }
    return menu;
}

/**
 * @brief Builds the submenu that lists every bindable action.
 * @return Submenu handle, ownership passes to the parent menu.
 */
static HMENU remap_menu(void)
{
    HMENU menu = CreatePopupMenu();
    for (int i = 0; i < ACTION_COUNT; i++) {
        wchar_t label[ACTION_NAME_CHARS];
        action_label((Action)i, label, ARRAYSIZE(label));
        AppendMenuW(menu, MF_STRING, (UINT_PTR)(ID_LEARN_BASE + i), label);
    }
    return menu;
}

/**
 * @brief Builds the context menu reflecting the given state.
 * @param state State to reflect.
 * @return Popup menu to be destroyed by the caller.
 */
static HMENU menu_build(const TrayState *state)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (state->drawing ? MF_CHECKED : 0), ID_TOGGLE_DRAWING, L"Drawing on/off");

    HMENU mode = CreatePopupMenu();
    AppendMenuW(mode, MF_STRING, ID_MODE_DESKTOP, L"Desktop");
    AppendMenuW(mode, MF_STRING, ID_MODE_WHITEBOARD, L"Whiteboard");
    CheckMenuRadioItem(mode, ID_MODE_DESKTOP, ID_MODE_WHITEBOARD,
                       state->whiteboard ? ID_MODE_WHITEBOARD : ID_MODE_DESKTOP, MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)mode, L"Mode");

    AppendMenuW(menu, MF_POPUP,
                (UINT_PTR)screens_menu(state->monitorCount, state->drawMonitor, ID_DRAW_MONITOR_BASE),
                L"Draw screen");
    AppendMenuW(menu, MF_POPUP,
                (UINT_PTR)screens_menu(state->monitorCount, state->uiMonitor, ID_UI_MONITOR_BASE),
                L"Tool panels screen");
    AppendMenuW(menu, MF_STRING | (state->hideUiFromCapture ? MF_CHECKED : 0), ID_HIDE_FROM_CAPTURE,
                L"Hide tools from screen capture");
    AppendMenuW(menu, MF_STRING | (state->autoHideUi ? MF_CHECKED : 0), ID_AUTO_HIDE, L"Auto-hide tools");
    if (state->learning) {
        AppendMenuW(menu, MF_STRING, ID_CANCEL_LEARN, L"Cancel learning");
    } else {
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)remap_menu(), L"Remap buttons");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_COPY_PNG, L"Copy drawing as PNG");
    AppendMenuW(menu, MF_STRING, ID_SAVE_PNG, L"Save drawing as PNG...");
    AppendMenuW(menu, MF_STRING, ID_RELOAD_CONFIG, L"Reload config");
    AppendMenuW(menu, MF_STRING, ID_OPEN_CONFIG, L"Open config file");
    AppendMenuW(menu, MF_STRING | (state->startWithWindows ? MF_CHECKED : 0), ID_AUTOSTART,
                L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit");
    return menu;
}

/**
 * @brief Translates a menu command id into a tray event.
 * @param id Chosen command id, 0 when the menu was dismissed.
 * @param event Receives the event.
 * @return true when the id maps to a command.
 */
static bool event_from_command(int id, TrayEvent *event)
{
    static const struct {
        int id;
        TrayCommand command;
    } simple[] = {
        {ID_TOGGLE_DRAWING, TRAY_TOGGLE_DRAWING},
        {ID_MODE_DESKTOP, TRAY_SET_DESKTOP_MODE},
        {ID_MODE_WHITEBOARD, TRAY_SET_WHITEBOARD_MODE},
        {ID_HIDE_FROM_CAPTURE, TRAY_TOGGLE_HIDE_UI_FROM_CAPTURE},
        {ID_AUTO_HIDE, TRAY_TOGGLE_AUTO_HIDE_UI},
        {ID_CANCEL_LEARN, TRAY_CANCEL_LEARN},
        {ID_COPY_PNG, TRAY_COPY_PNG},
        {ID_SAVE_PNG, TRAY_SAVE_PNG},
        {ID_RELOAD_CONFIG, TRAY_RELOAD_CONFIG},
        {ID_OPEN_CONFIG, TRAY_OPEN_CONFIG},
        {ID_AUTOSTART, TRAY_TOGGLE_START_WITH_WINDOWS},
        {ID_QUIT, TRAY_QUIT},
    };
    memset(event, 0, sizeof *event);
    for (size_t i = 0; i < ARRAYSIZE(simple); i++) {
        if (simple[i].id == id) {
            event->command = simple[i].command;
            return true;
        }
    }
    if (id >= ID_DRAW_MONITOR_BASE && id < ID_DRAW_MONITOR_BASE + TRAY_MAX_MONITORS) {
        event->command = TRAY_SET_DRAW_MONITOR;
        event->value = id - ID_DRAW_MONITOR_BASE;
        return true;
    }
    if (id >= ID_UI_MONITOR_BASE && id < ID_UI_MONITOR_BASE + TRAY_MAX_MONITORS) {
        event->command = TRAY_SET_UI_MONITOR;
        event->value = id - ID_UI_MONITOR_BASE;
        return true;
    }
    if (id >= ID_LEARN_BASE && id < ID_LEARN_BASE + ACTION_COUNT) {
        event->command = TRAY_LEARN_ACTION;
        event->action = (Action)(id - ID_LEARN_BASE);
        return true;
    }
    return false;
}

/**
 * @brief Shows the context menu at the cursor and returns the chosen command id.
 * @param tray Instance owning the notification window.
 * @param state State reflected by the menu.
 * @return Chosen command id, or 0 when dismissed.
 */
static int menu_run(const Tray *tray, const TrayState *state)
{
    HMENU menu = menu_build(state);
    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(tray->window);
    int id = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, cursor.x,
                                 cursor.y, 0, tray->window, NULL);
    PostMessageW(tray->window, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return id;
}

bool tray_handle_message(Tray *tray, LPARAM lparam, const TrayState *state, TrayEvent *event)
{
    UINT notification = LOWORD(lparam);
    memset(event, 0, sizeof *event);
    if (notification == NIN_SELECT || notification == NIN_KEYSELECT) {
        event->command = TRAY_TOGGLE_DRAWING;
        return true;
    }
    if (notification == WM_CONTEXTMENU) {
        return event_from_command(menu_run(tray, state), event);
    }
    return false;
}

/**
 * @brief Builds the quoted path of the running executable as stored in the Run key.
 * @param buffer Output buffer.
 * @param size Buffer size in characters.
 * @return true on success.
 */
static bool autostart_command(wchar_t *buffer, int size)
{
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    return swprintf(buffer, (size_t)size, L"\"%ls\"", path) > 0;
}

bool autostart_get(void)
{
    wchar_t expected[MAX_PATH + 4];
    wchar_t stored[MAX_PATH + 4];
    DWORD bytes = sizeof stored;
    if (!autostart_command(expected, ARRAYSIZE(expected))) {
        return false;
    }
    if (RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_KEY, AUTOSTART_VALUE, RRF_RT_REG_SZ, NULL, stored,
                     &bytes) != ERROR_SUCCESS) {
        return false;
    }
    return _wcsicmp(stored, expected) == 0;
}

bool autostart_set(bool enable)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key,
                        NULL) != ERROR_SUCCESS) {
        return false;
    }
    LONG result;
    if (enable) {
        wchar_t command[MAX_PATH + 4];
        if (!autostart_command(command, ARRAYSIZE(command))) {
            RegCloseKey(key);
            return false;
        }
        result = RegSetValueExW(key, AUTOSTART_VALUE, 0, REG_SZ, (const BYTE *)command,
                                (DWORD)((wcslen(command) + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, AUTOSTART_VALUE);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
