/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tool.h"

#include <string.h>
#include <strings.h>

typedef struct ToolInfo {
    const char *name;
    char letter;
} ToolInfo;

static const ToolInfo TOOL_INFO[TOOL_COUNT] = {
    {"select", 'V'}, {"rectangle", 'R'}, {"ellipse", 'O'}, {"arrow", 'A'}, {"line", 'L'},
    {"pen", 'P'},    {"laser", 'K'},     {"text", 'T'},    {"eraser", 'E'}, {"diamond", 'D'},
    {"highlighter", 'H'},
};

const char *tool_name(Tool tool)
{
    return (unsigned)tool < TOOL_COUNT ? TOOL_INFO[tool].name : "unknown";
}

bool tool_from_name(const char *name, Tool *tool)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (strcasecmp(name, TOOL_INFO[i].name) == 0) {
            *tool = (Tool)i;
            return true;
        }
    }
    return false;
}

static const Tool DEFAULT_ORDER[TOOL_COUNT] = {TOOL_SELECT, TOOL_PEN,     TOOL_HIGHLIGHTER, TOOL_ERASER,
                                               TOOL_TEXT,   TOOL_RECTANGLE, TOOL_ELLIPSE,   TOOL_DIAMOND,
                                               TOOL_ARROW,  TOOL_LINE,      TOOL_LASER};

void tool_order_default(Tool order[TOOL_COUNT])
{
    memcpy(order, DEFAULT_ORDER, sizeof DEFAULT_ORDER);
}

bool tool_mask_parse(const char *text, unsigned *mask)
{
    unsigned result = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        while (length > 0 && (*cursor == ' ' || *cursor == '\t')) {
            cursor++;
            length--;
        }
        while (length > 0 && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t')) {
            length--;
        }
        char name[32];
        Tool tool;
        if (length > 0) {
            if (length >= sizeof name) {
                return false;
            }
            memcpy(name, cursor, length);
            name[length] = '\0';
            if (!tool_from_name(name, &tool)) {
                return false;
            }
            result |= 1u << tool;
        }
        if (!end) {
            break;
        }
        cursor = end + 1;
    }
    if (result == (1u << TOOL_COUNT) - 1u) {
        return false;
    }
    *mask = result;
    return true;
}

bool tool_order_valid(const Tool order[TOOL_COUNT])
{
    bool seen[TOOL_COUNT] = {false};
    for (int i = 0; i < TOOL_COUNT; i++) {
        if ((unsigned)order[i] >= TOOL_COUNT || seen[order[i]]) {
            return false;
        }
        seen[order[i]] = true;
    }
    return true;
}

bool tool_order_parse(const char *text, Tool order[TOOL_COUNT])
{
    bool seen[TOOL_COUNT] = {false};
    int count = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        char name[32];
        while (length > 0 && (*cursor == ' ' || *cursor == '\t')) {
            cursor++;
            length--;
        }
        while (length > 0 && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t')) {
            length--;
        }
        Tool tool;
        if (length == 0 || length >= sizeof name) {
            return false;
        }
        memcpy(name, cursor, length);
        name[length] = '\0';
        if (!tool_from_name(name, &tool) || seen[tool]) {
            return false;
        }
        seen[tool] = true;
        order[count++] = tool;
        cursor = end ? end + 1 : cursor + length;
        if (!end) {
            break;
        }
    }
    if (count == 0) {
        return false;
    }
    for (int i = 0; i < TOOL_COUNT; i++) {
        Tool missing = DEFAULT_ORDER[i];
        if (seen[missing]) {
            continue;
        }
        int at = count;
        for (int j = 0; i > 0 && j < count; j++) {
            if (order[j] == DEFAULT_ORDER[i - 1]) {
                at = j + 1;
            }
        }
        memmove(&order[at + 1], &order[at], (size_t)(count - at) * sizeof order[0]);
        order[at] = missing;
        count++;
    }
    return true;
}

char tool_shortcut(Tool tool)
{
    return (unsigned)tool < TOOL_COUNT ? TOOL_INFO[tool].letter : 0;
}

bool tool_from_shortcut(int letter, Tool *tool)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (letter == TOOL_INFO[i].letter) {
            *tool = (Tool)i;
            return true;
        }
    }
    if (letter == 'X') {
        *tool = TOOL_PEN;
        return true;
    }
    return false;
}

bool tool_at_slot(const Tool order[TOOL_COUNT], unsigned hidden, int slot, Tool *tool)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        if ((hidden & (1u << order[i])) != 0) {
            continue;
        }
        if (slot-- == 0) {
            *tool = order[i];
            return true;
        }
    }
    return false;
}
