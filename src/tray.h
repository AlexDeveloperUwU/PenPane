/**
 * @file tray.h
 * @brief Notification-area icon and its context menu.
 */
#ifndef INK_TRAY_H
#define INK_TRAY_H

#include <stdbool.h>
#include <windows.h>

#include "actions.h"

#define TRAY_MAX_MONITORS 8

/** @brief State the menu reflects. */
typedef struct TrayState {
    bool drawing;
    bool whiteboard;
    int monitorCount;
    int drawMonitor;
    int uiMonitor;
    bool hideUiFromCapture;
    bool autoHideUi;
    bool startWithWindows;
    bool learning;
} TrayState;

/** @brief Commands the menu can produce. */
typedef enum TrayCommand {
    TRAY_NONE,
    TRAY_TOGGLE_DRAWING,
    TRAY_SET_DESKTOP_MODE,
    TRAY_SET_WHITEBOARD_MODE,
    TRAY_SET_DRAW_MONITOR,
    TRAY_SET_UI_MONITOR,
    TRAY_TOGGLE_HIDE_UI_FROM_CAPTURE,
    TRAY_TOGGLE_AUTO_HIDE_UI,
    TRAY_LEARN_ACTION,
    TRAY_CANCEL_LEARN,
    TRAY_RELOAD_CONFIG,
    TRAY_OPEN_CONFIG,
    TRAY_COPY_PNG,
    TRAY_TOGGLE_START_WITH_WINDOWS,
    TRAY_QUIT
} TrayCommand;

/** @brief A menu choice. value is a monitor index for the SET_*_MONITOR commands; action is set for TRAY_LEARN_ACTION. */
typedef struct TrayEvent {
    TrayCommand command;
    int value;
    Action action;
} TrayEvent;

/** @brief Opaque instance. */
typedef struct Tray Tray;

/**
 * @brief Adds the notification-area icon.
 * @param window Window that receives the callback message.
 * @param callbackMessage Message id (WM_APP based) the shell sends for icon events.
 * @return New instance, or NULL on failure.
 */
Tray *tray_create(HWND window, UINT callbackMessage);

/**
 * @brief Removes the icon and frees the instance.
 * @param tray Instance to destroy, may be NULL.
 */
void tray_destroy(Tray *tray);

/**
 * @brief Handles the shell callback message.
 *
 * Right click shows the menu (built from the given state) and returns the chosen command. A left
 * click toggles drawing. Re-adds the icon when the taskbar is recreated is handled by the owner
 * calling tray_restore.
 * @param tray Instance.
 * @param lparam LPARAM of the callback message.
 * @param state State reflected by the menu.
 * @param event Receives the chosen command when the function returns true.
 * @return true when the user chose a command.
 */
bool tray_handle_message(Tray *tray, LPARAM lparam, const TrayState *state, TrayEvent *event);

/**
 * @brief Re-adds the icon, for use after the "TaskbarCreated" broadcast message.
 * @param tray Instance.
 */
void tray_restore(Tray *tray);

/**
 * @brief Shows a balloon notification.
 * @param tray Instance.
 * @param title Title, at most 63 characters.
 * @param text Body, at most 255 characters.
 */
void tray_notify(Tray *tray, const wchar_t *title, const wchar_t *text);

/**
 * @brief Reads whether the program is registered to start with Windows (HKCU Run key).
 * @return true when registered.
 */
bool autostart_get(void);

/**
 * @brief Registers or unregisters the program in the HKCU Run key; never called without an explicit user action.
 * @param enable true to register the current executable path.
 * @return true on success.
 */
bool autostart_set(bool enable);

#endif
