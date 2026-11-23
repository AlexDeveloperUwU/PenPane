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
    TOOL_DIAMOND,
    TOOL_HIGHLIGHTER,
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
 * @brief Fills the default order in which the toolbar shows the tools.
 * @param order Receives every tool once.
 */
void tool_order_default(Tool order[TOOL_COUNT]);

/**
 * @brief Parses a comma separated list of tool names into a toolbar order.
 *
 * Tools missing from the list follow the listed ones in the default order.
 * @param text List such as "select, pen, eraser".
 * @param order Receives every tool once on success.
 * @return false when a name is unknown, repeated or the list is empty.
 */
bool tool_order_parse(const char *text, Tool order[TOOL_COUNT]);

/**
 * @brief Parses a comma separated list of tool names into a bitmask (bit n set for the tool with value n).
 * @param text List such as "laser, text"; may be empty.
 * @param mask Receives the mask on success.
 * @return false when a name is unknown or every tool would be hidden.
 */
bool tool_mask_parse(const char *text, unsigned *mask);

/**
 * @brief Tells whether an order lists every tool exactly once.
 * @param order Order to check.
 * @return true when valid.
 */
bool tool_order_valid(const Tool order[TOOL_COUNT]);

/**
 * @brief Returns the single-letter keyboard shortcut of a tool.
 * @param tool Tool to query.
 * @return Uppercase letter: V R O D A L P K T E H.
 */
char tool_shortcut(Tool tool);

/**
 * @brief Finds the tool bound to a shortcut letter (V R O D A L P K T E H, and X for the pen).
 * @param letter Uppercase letter.
 * @param tool Receives the tool on success.
 * @return true when a tool uses that letter.
 */
bool tool_from_shortcut(int letter, Tool *tool);

/**
 * @brief Finds the tool at a place of the toolbar, skipping hidden tools.
 *
 * The number keys 1 to 9 and 0 select places one to ten, so the shortcuts follow the toolbar order.
 * @param order Toolbar order.
 * @param hidden Bitmask of hidden tools.
 * @param slot Zero-based place among the visible tools.
 * @param tool Receives the tool on success.
 * @return false when fewer visible tools exist.
 */
bool tool_at_slot(const Tool order[TOOL_COUNT], unsigned hidden, int slot, Tool *tool);

#endif
