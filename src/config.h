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
#ifndef INK_CONFIG_H
#define INK_CONFIG_H

#include <stdbool.h>
#include <wchar.h>

#include "gesture.h"
#include "style.h"
#include "tool.h"
#include "ui.h"

#define CONFIG_MAX_BINDINGS GESTURE_MAX_BINDINGS

/** @brief Every persisted setting. */
typedef struct Config {
    Binding bindings[CONFIG_MAX_BINDINGS];
    int bindingCount;
    int doubleWindowMs;
    int longPressMs;
    Tool defaultTool;
    int defaultStrokeIndex;
    int defaultFillIndex;
    int defaultWidthIndex;
    int autoFadeSeconds;
    int drawMonitor;
    int uiMonitor;
    bool toolLock;
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
 * @param config Configuration to fill; reset to defaults first.
 * @param path File path.
 * @param errors Optional buffer receiving a short report of skipped lines, or NULL.
 * @param errorsSize Size of the errors buffer in characters.
 * @return true when the file was read, false when it does not exist or cannot be opened (defaults are kept).
 */
bool config_load(Config *config, const wchar_t *path, wchar_t *errors, int errorsSize);

/**
 * @brief Writes the configuration atomically (temporary file then rename).
 * @param config Configuration to write.
 * @param path Destination file path.
 * @return true on success.
 */
bool config_save(const Config *config, const wchar_t *path);

/**
 * @brief Replaces any binding with the same trigger and gesture, or appends a new one.
 * @param config Configuration to modify.
 * @param binding Binding to set.
 * @return true on success, false when the table is full.
 */
bool config_set_binding(Config *config, Binding binding);

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
