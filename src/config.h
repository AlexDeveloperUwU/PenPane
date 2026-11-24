/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file config.h
 * @brief Plain-text configuration file: bindings, timings, defaults and panel placement.
 *
 * The file is human readable, one `key = value` per line, `#` starts a comment line:
 *
 *     double_press_ms = 300
 *     long_press_ms = 600
 *     bind = F13 single undo
 *     bind = F13 double toggle_overlay
 *     bind = Ctrl+Alt+F14 long clear
 *     bind = XButton1 single next_tool
 *
 * Trigger syntax: optional `Ctrl+`, `Alt+`, `Shift+`, `Win+` prefixes then a key name (A-Z, 0-9,
 * F1-F24, Esc, Space, Enter, Tab, Home, End, PageUp, PageDown, Insert, Delete, Left, Right, Up,
 * Down, Numpad0-9, or `VK_0xNN` for any virtual-key code) or a mouse button name (MButton,
 * XButton1, XButton2).
 */
#ifndef PENPANE_CONFIG_H
#define PENPANE_CONFIG_H

#include <stdbool.h>
#include <wchar.h>

#include "gesture.h"
#include "style.h"
#include "tool.h"
#include "ui.h"

#define CONFIG_MAX_BINDINGS GESTURE_MAX_BINDINGS

/** @brief Outcome of reading the configuration file. */
typedef enum ConfigStatus { CONFIG_MISSING, CONFIG_LOADED, CONFIG_UNREADABLE } ConfigStatus;

/** @brief Every persisted setting. */
typedef struct Config {
    Binding bindings[CONFIG_MAX_BINDINGS];
    int bindingCount;
    int doubleWindowMs;
    int longPressMs;
    Tool defaultTool;
    Tool toolOrder[TOOL_COUNT];
    unsigned hiddenTools;
    int defaultStrokeIndex;
    int defaultFillIndex;
    int defaultWidthIndex;
    int autoFadeSeconds;
    int drawMonitor;
    int uiMonitor;
    bool toolLock;
    bool laserPointer;
    bool palettePastel;
    bool debugLog;
    bool exportBackground;
    bool exportFullScreen;
    Color customColors[CUSTOM_COLOR_MAX];
    int customCount;
    UiSettings ui;
} Config;

/**
 * @brief Fills a configuration with defaults, including the placeholder button bindings.
 *
 * Placeholders (to be changed by the user): F13 single = undo, F13 double = toggle_overlay,
 * F14 single = next_tool, F14 double = toggle_mode, F14 long = clear.
 * @param config Configuration to reset.
 */
void config_defaults(Config *config);

/**
 * @brief Loads a configuration file over the defaults; unknown or malformed lines are skipped.
 *
 * Only a file that does not exist counts as missing. Any other failure to read it (sharing
 * violation, access denied, I/O error) is reported as unreadable and leaves the configuration
 * untouched, so the caller never mistakes a locked file for an absent one and overwrites it.
 * @param config Configuration to fill; reset to defaults first unless the file is unreadable.
 * @param path File path.
 * @param errors Optional buffer receiving a short report of skipped lines, or NULL.
 * @param errorsSize Size of the errors buffer in characters.
 * @return CONFIG_LOADED when the file was read, CONFIG_MISSING when it does not exist (defaults applied),
 * CONFIG_UNREADABLE otherwise.
 */
ConfigStatus config_load(Config *config, const wchar_t *path, wchar_t *errors, int errorsSize);

/**
 * @brief Writes the configuration atomically (temporary file then rename).
 * @param config Configuration to write.
 * @param path Destination file path.
 * @return true on success.
 */
bool config_save(const Config *config, const wchar_t *path);

/**
 * @brief Copies a configuration file of the former product name to the current path, once.
 *
 * Nothing happens when the current file exists or cannot be told apart from missing, or when
 * the legacy file is absent. The legacy file is left in place.
 * @param legacyPath Path of the former configuration file.
 * @param path Path of the current configuration file.
 * @return true when the file was copied.
 */
bool config_migrate_legacy(const wchar_t *legacyPath, const wchar_t *path);

/**
 * @brief Replaces any binding with the same trigger and gesture, or appends a new one.
 * @param config Configuration to modify.
 * @param binding Binding to set.
 * @return true on success, false when the table is full or the trigger is not bindable.
 */
bool config_set_binding(Config *config, Binding binding);

/**
 * @brief Tells whether a trigger may be bound globally.
 *
 * A key that types text (A-Z, 0-9, Space, punctuation) or is needed for editing (Esc, Enter, Tab,
 * Backspace) is rejected unless Ctrl, Alt or Win is part of the trigger; Shift alone does not
 * count. Mouse buttons and the remaining keys are always allowed.
 * @param trigger Trigger to test.
 * @return true when the trigger can be registered without taking over ordinary typing.
 */
bool trigger_is_bindable(Trigger trigger);

/**
 * @brief Counts the distinct triggers used by the binding table.
 * @param config Configuration to inspect.
 * @return Number of different triggers; the gesture engine tracks at most GESTURE_MAX_TRACKED.
 */
int config_trigger_count(const Config *config);

/**
 * @brief Removes every binding of an action.
 * @param config Configuration to modify.
 * @param action Action whose bindings are removed.
 */
void config_clear_action(Config *config, Action action);

/**
 * @brief Formats a trigger as text such as "Ctrl+F13".
 * @param trigger Trigger to format.
 * @param buffer Output buffer.
 * @param size Buffer size in characters.
 */
void trigger_to_text(Trigger trigger, wchar_t *buffer, int size);

/**
 * @brief Parses trigger text such as "Ctrl+F13" or "XButton1".
 * @param text Text to parse.
 * @param trigger Receives the trigger on success.
 * @return true when the text is a valid trigger.
 */
bool trigger_from_text(const wchar_t *text, Trigger *trigger);

#endif
