/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file actions.h
 * @brief The complete list of actions that can be bound to a button or key.
 */
#ifndef PENPANE_ACTIONS_H
#define PENPANE_ACTIONS_H

#include <stdbool.h>

#define PENPANE_ACTION_LIST(X)                              \
    X(ACTION_TOGGLE_OVERLAY, "toggle_overlay")              \
    X(ACTION_TOGGLE_MODE, "toggle_mode")                    \
    X(ACTION_TOOL_SELECT, "tool_select")                    \
    X(ACTION_TOOL_RECTANGLE, "tool_rectangle")              \
    X(ACTION_TOOL_ELLIPSE, "tool_ellipse")                  \
    X(ACTION_TOOL_ARROW, "tool_arrow")                      \
    X(ACTION_TOOL_LINE, "tool_line")                        \
    X(ACTION_TOOL_PEN, "tool_pen")                          \
    X(ACTION_TOOL_LASER, "tool_laser")                      \
    X(ACTION_TOOL_TEXT, "tool_text")                        \
    X(ACTION_TOOL_ERASER, "tool_eraser")                    \
    X(ACTION_NEXT_TOOL, "next_tool")                        \
    X(ACTION_PREVIOUS_TOOL, "previous_tool")                \
    X(ACTION_CYCLE_COLOR, "cycle_color")                    \
    X(ACTION_CYCLE_WIDTH, "cycle_width")                    \
    X(ACTION_UNDO, "undo")                                  \
    X(ACTION_REDO, "redo")                                  \
    X(ACTION_CLEAR, "clear")                                \
    X(ACTION_TOGGLE_LASER, "toggle_laser")                  \
    X(ACTION_TOGGLE_TOOL_LOCK, "toggle_tool_lock")          \
    X(ACTION_COPY_PNG, "copy_png")                          \
    X(ACTION_SAVE_PNG, "save_png")                          \
    X(ACTION_EMERGENCY_EXIT, "emergency_exit")

/** @brief Every bindable action. */
typedef enum Action {
#define PENPANE_ACTION_ENUM(id, name) id,
    PENPANE_ACTION_LIST(PENPANE_ACTION_ENUM)
#undef PENPANE_ACTION_ENUM
    ACTION_COUNT
} Action;

/**
 * @brief Returns the configuration-file name of an action, for example "undo".
 * @param action Action to name.
 * @return Static string, or "unknown" for out-of-range values.
 */
const char *action_name(Action action);

/**
 * @brief Parses a configuration-file action name, case-insensitively.
 * @param name Text such as "toggle_mode".
 * @param action Receives the parsed action on success.
 * @return true when the name is known.
 */
bool action_from_name(const char *name, Action *action);

#endif
