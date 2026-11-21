/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file tool.h
 * @brief Drawing tools in toolbar order, with their Excalidraw shortcut letters.
 */
#ifndef PENPANE_TOOL_H
#define PENPANE_TOOL_H

#include <stdbool.h>

/** @brief Drawing tools in toolbar order. */
typedef enum Tool {
    TOOL_SELECT,
    TOOL_RECTANGLE,
    TOOL_ELLIPSE,
    TOOL_ARROW,
    TOOL_LINE,
    TOOL_PEN,
    TOOL_LASER,
    TOOL_TEXT,
    TOOL_ERASER,
    TOOL_COUNT
} Tool;

/**
 * @brief Returns the configuration name of a tool such as "pen".
 * @param tool Tool to name.
 * @return Static lowercase string.
 */
const char *tool_name(Tool tool);

/**
 * @brief Parses a tool name, case-insensitively.
 * @param name Text such as "rectangle".
 * @param tool Receives the tool on success.
 * @return true when the name is known.
 */
bool tool_from_name(const char *name, Tool *tool);

/**
 * @brief Returns the single-letter keyboard shortcut of a tool.
 * @param tool Tool to query.
 * @return Uppercase letter: V R O A L P K T E.
 */
char tool_shortcut(Tool tool);

/**
 * @brief Finds the tool bound to a shortcut letter.
 * @param letter Uppercase letter or digit as in Excalidraw (V/1, R/2, O/4, A/5, L/6, P/7, T/8, E/0, K).
 * @param tool Receives the tool on success.
 * @return true when a tool uses that key.
 */
bool tool_from_shortcut(int letter, Tool *tool);

#endif
