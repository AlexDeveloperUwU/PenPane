#include "tool.h"

#include <string.h>
#include <strings.h>

typedef struct ToolInfo {
    const char *name;
    char letter;
    char digit;
} ToolInfo;

static const ToolInfo TOOL_INFO[TOOL_COUNT] = {
    {"select", 'V', '1'}, {"rectangle", 'R', '2'}, {"ellipse", 'O', '4'},
    {"arrow", 'A', '5'},  {"line", 'L', '6'},      {"pen", 'P', '7'},
    {"laser", 'K', 0},    {"text", 'T', '8'},      {"eraser", 'E', '0'},
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

char tool_shortcut(Tool tool)
{
    return (unsigned)tool < TOOL_COUNT ? TOOL_INFO[tool].letter : 0;
}

bool tool_from_shortcut(int letter, Tool *tool)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (letter == TOOL_INFO[i].letter || (TOOL_INFO[i].digit && letter == TOOL_INFO[i].digit)) {
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
