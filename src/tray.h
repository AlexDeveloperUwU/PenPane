/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file tray.h
 * @brief Notification-area icon and its context menu.
 */
#ifndef PENPANE_TRAY_H
#define PENPANE_TRAY_H

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
    TRAY_SAVE_PNG,
    TRAY_TOGGLE_START_WITH_WINDOWS,
    TRAY_QUIT
} TrayCommand;

/** @brief A menu choice. value is a monitor index for the SET_*_MONITOR commands; action is set for
 * TRAY_LEARN_ACTION. */
typedef struct TrayEvent {
    TrayCommand command;
    int value;
    Action action;
} TrayEvent;

/** @brief Opaque instance. */
typedef struct Tray Tray;

/**
 * @brief Creates the notification-area icon object and tries to add it to the shell.
 *
 * The object exists even when the shell refuses the icon (for example because the taskbar has not
 * been created yet at logon); use tray_is_registered to see whether the icon is visible and
 * tray_retry or tray_restore to try again.
 * @param window Window that receives the callback message.
 * @param callbackMessage Message id (WM_APP based) the shell sends for icon events.
 * @return New instance, or NULL only when memory is exhausted.
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
 * @brief Adds the icon again, for use after the "TaskbarCreated" broadcast message.
 * @param tray Instance, may be NULL.
 * @return true when the icon is registered afterwards.
 */
bool tray_restore(Tray *tray);

/**
 * @brief Tries to register the icon when it is not registered yet; does nothing when it is.
 * @param tray Instance, may be NULL.
 * @return true when the icon is registered afterwards.
 */
bool tray_retry(Tray *tray);

/**
 * @brief Tells whether the shell currently holds the icon.
 * @param tray Instance, may be NULL.
 * @return true when the icon is registered.
 */
bool tray_is_registered(const Tray *tray);

/**
 * @brief Shows a balloon notification.
 * @param tray Instance.
 * @param title Title, at most 63 characters.
 * @param text Body, at most 255 characters.
 * @return false when the icon is not registered or the shell rejected the notification, so the caller can
 * keep the message.
 */
bool tray_notify(Tray *tray, const wchar_t *title, const wchar_t *text);

/**
 * @brief Reads whether the program is registered to start with Windows (HKCU Run key).
 * @return true when registered.
 */
bool autostart_get(void);

/**
 * @brief Registers or unregisters the program in the HKCU Run key; never called without an explicit user
 * action.
 * @param enable true to register the current executable path.
 * @return true on success.
 */
bool autostart_set(bool enable);

#endif
