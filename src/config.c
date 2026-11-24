/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <strings.h>
#include <string.h>
#include <windows.h>

#define CONFIG_MAX_FILE_BYTES 262144
#define CONFIG_OUTPUT_BYTES 8192
#define CONFIG_MAX_MONITOR 15

/** @brief Named key or mouse button of the trigger syntax. */
typedef struct NamedTrigger {
    const char *name;
    TriggerKind kind;
    unsigned virtualKey;
} NamedTrigger;

/** @brief Modifier prefix of the trigger syntax. */
typedef struct NamedModifier {
    const char *name;
    unsigned bit;
} NamedModifier;

/** @brief Bounded narrow-text writer used to build the file image. */
typedef struct TextBuffer {
    char *data;
    int size;
    int length;
    bool overflow;
} TextBuffer;

/** @brief Bounded wide-text writer used for the error report. */
typedef struct ErrorOut {
    wchar_t *data;
    int size;
    int length;
} ErrorOut;

/** @brief Keys and mouse buttons that have a fixed name. */
static const NamedTrigger NAMED_TRIGGERS[] = {
    {"Esc", TRIGGER_KEY, VK_ESCAPE},
    {"Space", TRIGGER_KEY, VK_SPACE},
    {"Enter", TRIGGER_KEY, VK_RETURN},
    {"Tab", TRIGGER_KEY, VK_TAB},
    {"Home", TRIGGER_KEY, VK_HOME},
    {"End", TRIGGER_KEY, VK_END},
    {"PageUp", TRIGGER_KEY, VK_PRIOR},
    {"PageDown", TRIGGER_KEY, VK_NEXT},
    {"Insert", TRIGGER_KEY, VK_INSERT},
    {"Delete", TRIGGER_KEY, VK_DELETE},
    {"Left", TRIGGER_KEY, VK_LEFT},
    {"Right", TRIGGER_KEY, VK_RIGHT},
    {"Up", TRIGGER_KEY, VK_UP},
    {"Down", TRIGGER_KEY, VK_DOWN},
    {"MButton", TRIGGER_MOUSE_BUTTON, VK_MBUTTON},
    {"XButton1", TRIGGER_MOUSE_BUTTON, VK_XBUTTON1},
    {"XButton2", TRIGGER_MOUSE_BUTTON, VK_XBUTTON2},
};

/** @brief Modifier prefixes in canonical output order. */
static const NamedModifier NAMED_MODIFIERS[] = {
    {"Ctrl", TRIGGER_MOD_CONTROL},
    {"Alt", TRIGGER_MOD_ALT},
    {"Shift", TRIGGER_MOD_SHIFT},
    {"Win", TRIGGER_MOD_WIN},
};

/** @brief Gesture names in Gesture enum order. */
static const char *const GESTURE_NAMES[] = {"single", "double", "long"};

/**
 * @brief Appends raw text to a writer, flagging overflow instead of truncating silently.
 * @param out Writer to extend.
 * @param text Zero-terminated text to append.
 */
static void out_text(TextBuffer *out, const char *text)
{
    int length = (int)strlen(text);
    if (out->length + length >= out->size) {
        out->overflow = true;
        return;
    }
    memcpy(out->data + out->length, text, (size_t)length + 1);
    out->length += length;
}

/**
 * @brief Appends a signed decimal number to a writer.
 * @param out Writer to extend.
 * @param value Number to append.
 */
static void out_int(TextBuffer *out, int value)
{
    char digits[16];
    int count = 0;
    unsigned magnitude = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    do {
        digits[count++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    char text[18];
    int length = 0;
    if (value < 0) {
        text[length++] = '-';
    }
    while (count > 0) {
        text[length++] = digits[--count];
    }
    text[length] = '\0';
    out_text(out, text);
}

/**
 * @brief Appends a boolean as "true" or "false".
 * @param out Writer to extend.
 * @param value Value to append.
 */
static void out_bool(TextBuffer *out, bool value)
{
    out_text(out, value ? "true" : "false");
}

/**
 * @brief Appends one `key = value` line with a numeric value.
 * @param out Writer to extend.
 * @param key Setting name.
 * @param value Number to write.
 */
static void out_setting_int(TextBuffer *out, const char *key, int value)
{
    out_text(out, key);
    out_text(out, " = ");
    out_int(out, value);
    out_text(out, "\n");
}

/**
 * @brief Appends one `key = value` line with a boolean value.
 * @param out Writer to extend.
 * @param key Setting name.
 * @param value Boolean to write.
 */
static void out_setting_bool(TextBuffer *out, const char *key, bool value)
{
    out_text(out, key);
    out_text(out, " = ");
    out_bool(out, value);
    out_text(out, "\n");
}

/**
 * @brief Appends a panel placement line when the placement is set.
 * @param out Writer to extend.
 * @param key Setting name such as "toolbar_pos".
 * @param placement Placement to write.
 */
static void out_placement(TextBuffer *out, const char *key, PanelPlacement placement)
{
    if (!placement.set) {
        return;
    }
    out_text(out, key);
    out_text(out, " = ");
    out_int(out, placement.x);
    out_text(out, ",");
    out_int(out, placement.y);
    out_text(out, "\n");
}

/**
 * @brief Appends a line to the error report, silently stopping when the buffer is full.
 * @param out Report writer; ignored when it has no buffer.
 * @param line One-based line number of the offending input line.
 * @param reason Short ASCII reason.
 */
static void report_error(ErrorOut *out, int line, const char *reason)
{
    if (out->data == NULL || out->size <= 0) {
        return;
    }
    char text[96];
    TextBuffer narrow = {text, (int)sizeof text, 0, false};
    text[0] = '\0';
    out_text(&narrow, "line ");
    out_int(&narrow, line);
    out_text(&narrow, ": ");
    out_text(&narrow, reason);
    out_text(&narrow, "\n");
    if (narrow.overflow || out->length + narrow.length >= out->size) {
        return;
    }
    for (int i = 0; i < narrow.length; i++) {
        out->data[out->length++] = (wchar_t)(unsigned char)text[i];
    }
    out->data[out->length] = L'\0';
}

/**
 * @brief Returns true for the ASCII whitespace characters allowed around tokens.
 * @param c Character to test.
 * @return true for space, tab, carriage return and line feed.
 */
static bool is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/**
 * @brief Trims leading and trailing whitespace in place.
 * @param text Writable zero-terminated text.
 * @return Pointer to the first non-blank character inside the same buffer.
 */
static char *trim(char *text)
{
    while (is_blank(*text)) {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && is_blank(text[length - 1])) {
        text[--length] = '\0';
    }
    return text;
}

/**
 * @brief Parses a decimal integer within a range, rejecting any trailing characters.
 * @param text Zero-terminated text.
 * @param minimum Smallest accepted value.
 * @param maximum Largest accepted value.
 * @param value Receives the number on success.
 * @return true when the text is an integer inside the range.
 */
static bool parse_int(const char *text, int minimum, int maximum, int *value)
{
    bool negative = false;
    if (*text == '-') {
        negative = true;
        text++;
    }
    if (*text == '\0') {
        return false;
    }
    long long result = 0;
    for (; *text != '\0'; text++) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        result = result * 10 + (*text - '0');
        if (result > 1000000000LL) {
            return false;
        }
    }
    if (negative) {
        result = -result;
    }
    if (result < minimum || result > maximum) {
        return false;
    }
    *value = (int)result;
    return true;
}

/**
 * @brief Parses a boolean written as true/false, yes/no, on/off or 1/0.
 * @param text Zero-terminated text.
 * @param value Receives the boolean on success.
 * @return true when the text is a boolean.
 */
static bool parse_bool(const char *text, bool *value)
{
    static const char *const TRUE_WORDS[] = {"true", "yes", "on", "1"};
    static const char *const FALSE_WORDS[] = {"false", "no", "off", "0"};
    for (int i = 0; i < 4; i++) {
        if (strcasecmp(text, TRUE_WORDS[i]) == 0) {
            *value = true;
            return true;
        }
        if (strcasecmp(text, FALSE_WORDS[i]) == 0) {
            *value = false;
            return true;
        }
    }
    return false;
}

/**
 * @brief Parses a panel placement written as "x,y".
 * @param text Writable zero-terminated text.
 * @param placement Receives the placement on success.
 * @return true when both coordinates are valid.
 */
static bool parse_placement(char *text, PanelPlacement *placement)
{
    char *comma = strchr(text, ',');
    if (comma == NULL) {
        return false;
    }
    *comma = '\0';
    int x;
    int y;
    if (!parse_int(trim(text), -100000, 100000, &x) || !parse_int(trim(comma + 1), -100000, 100000, &y)) {
        return false;
    }
    placement->set = true;
    placement->x = x;
    placement->y = y;
    return true;
}

/**
 * @brief Parses a hexadecimal byte after the "VK_0x" prefix.
 * @param text Hex digits.
 * @param value Receives a value from 1 to 0xFE.
 * @return true when the text is one or two hex digits in range.
 */
static bool parse_hex_key(const char *text, unsigned *value)
{
    unsigned result = 0;
    int digits = 0;
    for (; *text != '\0'; text++, digits++) {
        char c = *text;
        unsigned digit;
        if (c >= '0' && c <= '9') {
            digit = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = (unsigned)(c - 'A' + 10);
        } else {
            return false;
        }
        result = result * 16u + digit;
    }
    if (digits < 1 || digits > 2 || result == 0 || result > 0xFEu) {
        return false;
    }
    *value = result;
    return true;
}

/**
 * @brief Parses the key part of a trigger, without modifiers.
 * @param text Key or mouse button name.
 * @param trigger Receives kind and virtual key; modifiers are untouched.
 * @return true when the name is valid.
 */
static bool parse_key_name(const char *text, Trigger *trigger)
{
    size_t length = strlen(text);
    for (size_t i = 0; i < sizeof NAMED_TRIGGERS / sizeof NAMED_TRIGGERS[0]; i++) {
        if (strcasecmp(text, NAMED_TRIGGERS[i].name) == 0) {
            trigger->kind = NAMED_TRIGGERS[i].kind;
            trigger->virtualKey = NAMED_TRIGGERS[i].virtualKey;
            return true;
        }
    }
    trigger->kind = TRIGGER_KEY;
    if (length == 1 && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z') ||
                        (text[0] >= '0' && text[0] <= '9'))) {
        char c = text[0];
        trigger->virtualKey = (unsigned)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
        return true;
    }
    int number;
    if ((text[0] == 'F' || text[0] == 'f') && parse_int(text + 1, 1, 24, &number)) {
        trigger->virtualKey = VK_F1 + (unsigned)number - 1u;
        return true;
    }
    if (length == 7 && strncasecmp(text, "Numpad", 6) == 0 && text[6] >= '0' && text[6] <= '9') {
        trigger->virtualKey = VK_NUMPAD0 + (unsigned)(text[6] - '0');
        return true;
    }
    if (length > 5 && strncasecmp(text, "VK_0x", 5) == 0) {
        return parse_hex_key(text + 5, &trigger->virtualKey);
    }
    return false;
}

/**
 * @brief Parses a modifier prefix name.
 * @param text Name such as "Ctrl".
 * @param bit Receives the TRIGGER_MOD_* bit.
 * @return true when the name is a modifier.
 */
static bool parse_modifier(const char *text, unsigned *bit)
{
    for (size_t i = 0; i < sizeof NAMED_MODIFIERS / sizeof NAMED_MODIFIERS[0]; i++) {
        if (strcasecmp(text, NAMED_MODIFIERS[i].name) == 0) {
            *bit = NAMED_MODIFIERS[i].bit;
            return true;
        }
    }
    if (strcasecmp(text, "Control") == 0) {
        *bit = TRIGGER_MOD_CONTROL;
        return true;
    }
    return false;
}

/**
 * @brief Parses narrow trigger text.
 * @param text Writable zero-terminated text, modified in place.
 * @param trigger Receives the trigger on success.
 * @return true when the text is a valid trigger.
 */
static bool parse_trigger_narrow(char *text, Trigger *trigger)
{
    Trigger result = {TRIGGER_KEY, 0, 0};
    char *part = text;
    for (;;) {
        char *plus = strchr(part, '+');
        if (plus == NULL) {
            break;
        }
        *plus = '\0';
        unsigned bit;
        if (!parse_modifier(trim(part), &bit)) {
            return false;
        }
        result.modifiers |= bit;
        part = plus + 1;
    }
    if (!parse_key_name(trim(part), &result)) {
        return false;
    }
    *trigger = result;
    return true;
}

/**
 * @brief Looks up a gesture by name.
 * @param text Name such as "double".
 * @param gesture Receives the gesture on success.
 * @return true when the name is known.
 */
static bool parse_gesture(const char *text, Gesture *gesture)
{
    for (int i = 0; i < 3; i++) {
        if (strcasecmp(text, GESTURE_NAMES[i]) == 0) {
            *gesture = (Gesture)i;
            return true;
        }
    }
    return false;
}

/**
 * @brief Parses the value of a `bind` line: trigger, gesture and action separated by blanks.
 * @param value Writable value text.
 * @param binding Receives the binding on success.
 * @return true when all three parts are valid.
 */
static bool parse_binding(char *value, Binding *binding)
{
    char *fields[3];
    int count = 0;
    char *cursor = value;
    while (*cursor != '\0' && count < 3) {
        while (is_blank(*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        fields[count++] = cursor;
        while (*cursor != '\0' && !is_blank(*cursor)) {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    if (count != 3 || *trim(cursor) != '\0') {
        return false;
    }
    return parse_trigger_narrow(fields[0], &binding->trigger) &&
           parse_gesture(fields[1], &binding->gesture) && action_from_name(fields[2], &binding->action);
}

/**
 * @brief Applies one `key = value` pair to the configuration.
 * @param config Configuration to modify.
 * @param key Setting name, trimmed.
 * @param value Setting value, trimmed and writable.
 * @param reason Receives a static explanation when the pair is rejected.
 * @return true when the pair was recognized and valid.
 */
static bool apply_setting(Config *config, const char *key, char *value, const char **reason)
{
    static const char *const PLACEMENT_KEYS[PANEL_COUNT] = {"toolbar_pos", "properties_pos", "history_pos"};
    static const char *const PINNED_KEYS[PANEL_COUNT] = {"toolbar_pinned", "properties_pinned",
                                                         "history_pinned"};
    int number;
    bool flag;
    *reason = "invalid value";
    if (strcasecmp(key, "double_press_ms") == 0) {
        if (!parse_int(value, 50, 2000, &number)) {
            return false;
        }
        config->doubleWindowMs = number;
    } else if (strcasecmp(key, "long_press_ms") == 0) {
        if (!parse_int(value, 200, 5000, &number)) {
            return false;
        }
        config->longPressMs = number;
    } else if (strcasecmp(key, "default_tool") == 0) {
        return tool_from_name(value, &config->defaultTool);
    } else if (strcasecmp(key, "tool_order") == 0) {
        Tool order[TOOL_COUNT];
        if (!tool_order_parse(value, order)) {
            return false;
        }
        memcpy(config->toolOrder, order, sizeof order);
    } else if (strcasecmp(key, "hidden_tools") == 0) {
        return tool_mask_parse(value, &config->hiddenTools);
    } else if (strcasecmp(key, "default_stroke_index") == 0) {
        if (!parse_int(value, 0, STROKE_PALETTE_SIZE - 1, &number)) {
            return false;
        }
        config->defaultStrokeIndex = number;
    } else if (strcasecmp(key, "default_fill_index") == 0) {
        if (!parse_int(value, 0, FILL_PALETTE_SIZE - 1, &number)) {
            return false;
        }
        config->defaultFillIndex = number;
    } else if (strcasecmp(key, "default_width_index") == 0) {
        if (!parse_int(value, 0, WIDTH_PRESET_COUNT - 1, &number)) {
            return false;
        }
        config->defaultWidthIndex = number;
    } else if (strcasecmp(key, "auto_fade_seconds") == 0) {
        if (!parse_int(value, 0, 3600, &number)) {
            return false;
        }
        config->autoFadeSeconds = number;
    } else if (strcasecmp(key, "draw_monitor") == 0) {
        if (!parse_int(value, 0, CONFIG_MAX_MONITOR, &number)) {
            return false;
        }
        config->drawMonitor = number;
    } else if (strcasecmp(key, "ui_monitor") == 0) {
        if (!parse_int(value, 0, CONFIG_MAX_MONITOR, &number)) {
            return false;
        }
        config->uiMonitor = number;
    } else if (strcasecmp(key, "tool_lock") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->toolLock = flag;
    } else if (strcasecmp(key, "laser_pointer") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->laserPointer = flag;
    } else if (strcasecmp(key, "palette_pastel") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->palettePastel = flag;
    } else if (strcasecmp(key, "debug_log") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->debugLog = flag;
    } else if (strcasecmp(key, "export_background") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->exportBackground = flag;
    } else if (strcasecmp(key, "export_full_screen") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->exportFullScreen = flag;
    } else if (strcasecmp(key, "custom_color") == 0) {
        Color color;
        if (!color_parse_hex(value, &color)) {
            *reason = "expected #RRGGBB";
            return false;
        }
        if (config->customCount >= CUSTOM_COLOR_MAX) {
            *reason = "too many custom colors";
            return false;
        }
        config->customColors[config->customCount++] = color;
    } else if (strcasecmp(key, "ui_auto_hide") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->ui.autoHide = flag;
    } else if (strcasecmp(key, "ui_auto_hide_ms") == 0) {
        if (!parse_int(value, 500, 60000, &number)) {
            return false;
        }
        config->ui.autoHideMs = number;
    } else if (strcasecmp(key, "ui_exclude_from_capture") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        config->ui.excludeFromCapture = flag;
    } else if (strcasecmp(key, "reset_bindings") == 0) {
        if (!parse_bool(value, &flag)) {
            return false;
        }
        if (flag) {
            config->bindingCount = 0;
        }
    } else if (strcasecmp(key, "bind") == 0) {
        Binding binding;
        if (!parse_binding(value, &binding)) {
            *reason = "invalid binding";
            return false;
        }
        if (!trigger_is_bindable(binding.trigger)) {
            *reason = "plain typing key needs Ctrl, Alt or Win";
            return false;
        }
        if (!config_set_binding(config, binding)) {
            *reason = "too many bindings";
            return false;
        }
    } else {
        for (int i = 0; i < PANEL_COUNT; i++) {
            if (strcasecmp(key, PLACEMENT_KEYS[i]) == 0) {
                return parse_placement(value, &config->ui.placement[i]);
            }
            if (strcasecmp(key, PINNED_KEYS[i]) == 0) {
                if (!parse_bool(value, &flag)) {
                    return false;
                }
                config->ui.pinned[i] = flag;
                return true;
            }
        }
        *reason = "unknown key";
        return false;
    }
    return true;
}

/**
 * @brief Parses one line of the file and applies it.
 * @param config Configuration to modify.
 * @param line Writable zero-terminated line without line break.
 * @param lineNumber One-based line number for the report.
 * @param errors Report writer.
 */
static void parse_line(Config *config, char *line, int lineNumber, ErrorOut *errors)
{
    char *text = trim(line);
    if (*text == '\0' || *text == '#') {
        return;
    }
    char *equals = strchr(text, '=');
    if (equals == NULL) {
        report_error(errors, lineNumber, "missing '='");
        return;
    }
    *equals = '\0';
    const char *reason;
    if (!apply_setting(config, trim(text), trim(equals + 1), &reason)) {
        report_error(errors, lineNumber, reason);
    }
}

/**
 * @brief Reads a whole file into a heap buffer and appends a terminating zero.
 * @param path File path.
 * @param length Receives the number of bytes read.
 * @param error Receives the Win32 error code when the file cannot be read.
 * @return Heap buffer to release with HeapFree, or NULL when the file cannot be read.
 */
static char *read_file(const wchar_t *path, DWORD *length, DWORD *error)
{
    *error = ERROR_SUCCESS;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        *error = GetLastError();
        return NULL;
    }
    LARGE_INTEGER size;
    char *buffer = NULL;
    if (!GetFileSizeEx(file, &size)) {
        *error = GetLastError();
    } else {
        DWORD wanted = size.QuadPart > CONFIG_MAX_FILE_BYTES ? CONFIG_MAX_FILE_BYTES : (DWORD)size.QuadPart;
        buffer = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)wanted + 1);
        DWORD got = 0;
        if (buffer == NULL) {
            *error = ERROR_NOT_ENOUGH_MEMORY;
        } else if (!ReadFile(file, buffer, wanted, &got, NULL)) {
            *error = GetLastError();
            HeapFree(GetProcessHeap(), 0, buffer);
            buffer = NULL;
        } else {
            buffer[got] = '\0';
            *length = got;
        }
    }
    CloseHandle(file);
    return buffer;
}

void config_defaults(Config *config)
{
    memset(config, 0, sizeof *config);
    config->doubleWindowMs = 300;
    config->longPressMs = 600;
    config->defaultTool = TOOL_SELECT;
    tool_order_default(config->toolOrder);
    config->hiddenTools = 1u << TOOL_DIAMOND;
    config->defaultWidthIndex = 1;
    config->toolLock = true;
    config->laserPointer = false;
    config->ui.autoHide = true;
    config->ui.autoHideMs = 1200;
    for (int i = 0; i < PANEL_COUNT; i++) {
        config->ui.pinned[i] = true;
    }
    config->ui.excludeFromCapture = true;
    config->exportBackground = true;
    config->exportFullScreen = true;
    const Binding defaults[] = {
        {{TRIGGER_KEY, VK_F13, 0}, GESTURE_SINGLE, ACTION_UNDO},
        {{TRIGGER_KEY, VK_F13, 0}, GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY},
        {{TRIGGER_KEY, VK_F14, 0}, GESTURE_SINGLE, ACTION_NEXT_TOOL},
        {{TRIGGER_KEY, VK_F14, 0}, GESTURE_DOUBLE, ACTION_TOGGLE_MODE},
        {{TRIGGER_KEY, VK_F14, 0}, GESTURE_LONG, ACTION_CLEAR},
    };
    for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; i++) {
        config->bindings[config->bindingCount++] = defaults[i];
    }
}

ConfigStatus config_load(Config *config, const wchar_t *path, wchar_t *errors, int errorsSize)
{
    ErrorOut report = {errors, errorsSize, 0};
    if (errors != NULL && errorsSize > 0) {
        errors[0] = L'\0';
    }
    DWORD length = 0;
    DWORD error = ERROR_SUCCESS;
    char *data = read_file(path, &length, &error);
    if (data == NULL) {
        if (error == ERROR_FILE_NOT_FOUND) {
            config_defaults(config);
            return CONFIG_MISSING;
        }
        return CONFIG_UNREADABLE;
    }
    config_defaults(config);
    char *cursor = data;
    if (length >= 3 && (unsigned char)cursor[0] == 0xEF && (unsigned char)cursor[1] == 0xBB &&
        (unsigned char)cursor[2] == 0xBF) {
        cursor += 3;
    }
    int lineNumber = 1;
    while (*cursor != '\0') {
        char *end = cursor;
        while (*end != '\0' && *end != '\n') {
            end++;
        }
        bool last = *end == '\0';
        *end = '\0';
        parse_line(config, cursor, lineNumber++, &report);
        cursor = last ? end : end + 1;
    }
    HeapFree(GetProcessHeap(), 0, data);
    return CONFIG_LOADED;
}

/**
 * @brief Builds the text image of a configuration.
 * @param config Configuration to serialize.
 * @param out Writer receiving the text.
 */
static void format_config(const Config *config, TextBuffer *out)
{
    static const char *const PLACEMENT_KEYS[PANEL_COUNT] = {"toolbar_pos", "properties_pos", "history_pos"};
    static const char *const PINNED_KEYS[PANEL_COUNT] = {"toolbar_pinned", "properties_pinned",
                                                         "history_pinned"};
    out_text(out, "# Configuration. Unknown or malformed lines are ignored.\n");
    out_setting_int(out, "double_press_ms", config->doubleWindowMs);
    out_setting_int(out, "long_press_ms", config->longPressMs);
    out_text(out, "default_tool = ");
    out_text(out, tool_name(config->defaultTool));
    out_text(out, "\n");
    out_text(out, "tool_order = ");
    for (int i = 0; i < TOOL_COUNT; i++) {
        out_text(out, i > 0 ? ", " : "");
        out_text(out, tool_name(config->toolOrder[i]));
    }
    out_text(out, "\n");
    out_text(out, "hidden_tools = ");
    bool firstHidden = true;
    for (int i = 0; i < TOOL_COUNT; i++) {
        if ((config->hiddenTools & (1u << i)) != 0) {
            out_text(out, firstHidden ? "" : ", ");
            out_text(out, tool_name((Tool)i));
            firstHidden = false;
        }
    }
    out_text(out, "\n");
    out_setting_int(out, "default_stroke_index", config->defaultStrokeIndex);
    out_setting_int(out, "default_fill_index", config->defaultFillIndex);
    out_setting_int(out, "default_width_index", config->defaultWidthIndex);
    out_setting_int(out, "auto_fade_seconds", config->autoFadeSeconds);
    out_setting_int(out, "draw_monitor", config->drawMonitor);
    out_setting_int(out, "ui_monitor", config->uiMonitor);
    out_setting_bool(out, "tool_lock", config->toolLock);
    out_setting_bool(out, "laser_pointer", config->laserPointer);
    out_setting_bool(out, "palette_pastel", config->palettePastel);
    out_setting_bool(out, "debug_log", config->debugLog);
    out_setting_bool(out, "export_background", config->exportBackground);
    out_setting_bool(out, "export_full_screen", config->exportFullScreen);
    for (int i = 0; i < config->customCount; i++) {
        static const char HEX[] = "0123456789ABCDEF";
        Color color = config->customColors[i];
        char text[10] = {'#', '\0'};
        for (int digit = 0; digit < 6; digit++) {
            text[1 + digit] = HEX[(color >> (20 - 4 * digit)) & 0xFu];
        }
        text[7] = '\n';
        text[8] = '\0';
        out_text(out, "custom_color = ");
        out_text(out, text);
    }
    out_setting_bool(out, "ui_auto_hide", config->ui.autoHide);
    out_setting_int(out, "ui_auto_hide_ms", config->ui.autoHideMs);
    out_setting_bool(out, "ui_exclude_from_capture", config->ui.excludeFromCapture);
    for (int i = 0; i < PANEL_COUNT; i++) {
        out_placement(out, PLACEMENT_KEYS[i], config->ui.placement[i]);
    }
    for (int i = 0; i < PANEL_COUNT; i++) {
        out_setting_bool(out, PINNED_KEYS[i], config->ui.pinned[i]);
    }
    out_setting_bool(out, "reset_bindings", true);
    for (int i = 0; i < config->bindingCount; i++) {
        const Binding *binding = &config->bindings[i];
        wchar_t wide[64];
        char narrow[64];
        trigger_to_text(binding->trigger, wide, 64);
        int j = 0;
        for (; wide[j] != L'\0' && j < 63; j++) {
            narrow[j] = (char)wide[j];
        }
        narrow[j] = '\0';
        out_text(out, "bind = ");
        out_text(out, narrow);
        out_text(out, " ");
        out_text(out, (unsigned)binding->gesture < 3 ? GESTURE_NAMES[binding->gesture] : "single");
        out_text(out, " ");
        out_text(out, action_name(binding->action));
        out_text(out, "\n");
    }
}

bool config_save(const Config *config, const wchar_t *path)
{
    char image[CONFIG_OUTPUT_BYTES];
    TextBuffer out = {image, (int)sizeof image, 0, false};
    image[0] = '\0';
    format_config(config, &out);
    if (out.overflow) {
        return false;
    }
    size_t pathLength = wcslen(path);
    wchar_t temporary[MAX_PATH + 8];
    if (pathLength > MAX_PATH - 1) {
        return false;
    }
    memcpy(temporary, path, pathLength * sizeof(wchar_t));
    memcpy(temporary + pathLength, L".tmp", 5 * sizeof(wchar_t));
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, image, (DWORD)out.length, &written, NULL) && written == (DWORD)out.length &&
              FlushFileBuffers(file);
    CloseHandle(file);
    if (ok) {
        ok = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!ok) {
        DeleteFileW(temporary);
    }
    return ok;
}

bool config_migrate_legacy(const wchar_t *legacyPath, const wchar_t *path)
{
    DWORD current = GetFileAttributesW(path);
    if (current != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND) {
        return false;
    }
    DWORD legacy = GetFileAttributesW(legacyPath);
    if (legacy == INVALID_FILE_ATTRIBUTES || (legacy & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }
    return CopyFileW(legacyPath, path, TRUE) != 0;
}

bool trigger_is_bindable(Trigger trigger)
{
    if (trigger.kind != TRIGGER_KEY) {
        return true;
    }
    if ((trigger.modifiers & (TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT | TRIGGER_MOD_WIN)) != 0) {
        return true;
    }
    unsigned vk = trigger.virtualKey;
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        return false;
    }
    if (vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_TAB || vk == VK_SPACE || vk == VK_BACK) {
        return false;
    }
    if ((vk >= VK_OEM_1 && vk <= VK_OEM_3) || (vk >= VK_OEM_4 && vk <= VK_OEM_8) || vk == VK_OEM_102) {
        return false;
    }
    return true;
}

int config_trigger_count(const Config *config)
{
    int distinct = 0;
    for (int i = 0; i < config->bindingCount; i++) {
        bool seen = false;
        for (int j = 0; j < i && !seen; j++) {
            seen = trigger_equals(config->bindings[j].trigger, config->bindings[i].trigger);
        }
        if (!seen) {
            distinct++;
        }
    }
    return distinct;
}

bool config_set_binding(Config *config, Binding binding)
{
    if (!trigger_is_bindable(binding.trigger)) {
        return false;
    }
    for (int i = 0; i < config->bindingCount; i++) {
        Binding *existing = &config->bindings[i];
        if (existing->gesture == binding.gesture && trigger_equals(existing->trigger, binding.trigger)) {
            existing->action = binding.action;
            return true;
        }
    }
    if (config->bindingCount >= CONFIG_MAX_BINDINGS) {
        return false;
    }
    config->bindings[config->bindingCount++] = binding;
    return true;
}

void config_clear_action(Config *config, Action action)
{
    int kept = 0;
    for (int i = 0; i < config->bindingCount; i++) {
        if (config->bindings[i].action != action) {
            config->bindings[kept++] = config->bindings[i];
        }
    }
    config->bindingCount = kept;
}

void trigger_to_text(Trigger trigger, wchar_t *buffer, int size)
{
    if (buffer == NULL || size <= 0) {
        return;
    }
    char text[64];
    TextBuffer out = {text, (int)sizeof text, 0, false};
    text[0] = '\0';
    for (size_t i = 0; i < sizeof NAMED_MODIFIERS / sizeof NAMED_MODIFIERS[0]; i++) {
        if ((trigger.modifiers & NAMED_MODIFIERS[i].bit) != 0) {
            out_text(&out, NAMED_MODIFIERS[i].name);
            out_text(&out, "+");
        }
    }
    unsigned vk = trigger.virtualKey;
    bool named = false;
    for (size_t i = 0; i < sizeof NAMED_TRIGGERS / sizeof NAMED_TRIGGERS[0] && !named; i++) {
        if (NAMED_TRIGGERS[i].kind == trigger.kind && NAMED_TRIGGERS[i].virtualKey == vk) {
            out_text(&out, NAMED_TRIGGERS[i].name);
            named = true;
        }
    }
    if (!named && trigger.kind == TRIGGER_KEY) {
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
            char letter[2] = {(char)vk, '\0'};
            out_text(&out, letter);
            named = true;
        } else if (vk >= VK_F1 && vk <= VK_F24) {
            out_text(&out, "F");
            out_int(&out, (int)(vk - VK_F1) + 1);
            named = true;
        } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
            out_text(&out, "Numpad");
            out_int(&out, (int)(vk - VK_NUMPAD0));
            named = true;
        }
    }
    if (!named) {
        static const char HEX[] = "0123456789ABCDEF";
        char code[3] = {HEX[(vk >> 4) & 0xFu], HEX[vk & 0xFu], '\0'};
        out_text(&out, "VK_0x");
        out_text(&out, code);
    }
    int i = 0;
    for (; text[i] != '\0' && i < size - 1; i++) {
        buffer[i] = (wchar_t)(unsigned char)text[i];
    }
    buffer[i] = L'\0';
}

bool trigger_from_text(const wchar_t *text, Trigger *trigger)
{
    if (text == NULL || trigger == NULL) {
        return false;
    }
    char narrow[64];
    size_t i = 0;
    for (; text[i] != L'\0'; i++) {
        if (i >= sizeof narrow - 1 || text[i] > 126 || text[i] < 32) {
            return false;
        }
        narrow[i] = (char)text[i];
    }
    narrow[i] = '\0';
    return parse_trigger_narrow(narrow, trigger);
}
