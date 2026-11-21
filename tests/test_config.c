/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../src/config.h"

/**
 * @brief Builds a path of a scratch file in the temporary directory.
 * @param name File name.
 * @param path Receives the full path.
 * @param size Capacity of path in characters.
 */
static void scratch_path(const wchar_t *name, wchar_t *path, int size)
{
    DWORD length = GetTempPathW((DWORD)size, path);
    assert(length > 0 && (int)length + (int)wcslen(name) < size);
    wcscat(path, name);
}

/**
 * @brief Writes raw bytes to a file, replacing it.
 * @param path Destination path.
 * @param text Zero-terminated bytes to write.
 */
static void write_text(const wchar_t *path, const char *text)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    DWORD written;
    assert(WriteFile(file, text, (DWORD)strlen(text), &written, NULL));
    CloseHandle(file);
}

/**
 * @brief Builds a keyboard trigger.
 * @param virtualKey Virtual-key code.
 * @param modifiers TRIGGER_MOD_* bits.
 * @return Trigger value.
 */
static Trigger key(unsigned virtualKey, unsigned modifiers)
{
    Trigger trigger = {TRIGGER_KEY, virtualKey, modifiers};
    return trigger;
}

/**
 * @brief Compares two configurations field by field.
 * @param a First configuration.
 * @param b Second configuration.
 * @return true when every persisted field matches.
 */
static bool config_equal(const Config *a, const Config *b)
{
    if (a->bindingCount != b->bindingCount || a->doubleWindowMs != b->doubleWindowMs ||
        a->longPressMs != b->longPressMs || a->defaultTool != b->defaultTool ||
        a->defaultStrokeIndex != b->defaultStrokeIndex || a->defaultFillIndex != b->defaultFillIndex ||
        a->defaultWidthIndex != b->defaultWidthIndex || a->autoFadeSeconds != b->autoFadeSeconds ||
        a->drawMonitor != b->drawMonitor || a->uiMonitor != b->uiMonitor || a->toolLock != b->toolLock ||
        a->ui.autoHide != b->ui.autoHide || a->ui.autoHideMs != b->ui.autoHideMs ||
        a->ui.excludeFromCapture != b->ui.excludeFromCapture) {
        return false;
    }
    for (int i = 0; i < PANEL_COUNT; i++) {
        const PanelPlacement *p = &a->ui.placement[i];
        const PanelPlacement *q = &b->ui.placement[i];
        if (a->ui.pinned[i] != b->ui.pinned[i]) {
            return false;
        }
        if (p->set != q->set || (p->set && (p->x != q->x || p->y != q->y))) {
            return false;
        }
    }
    for (int i = 0; i < a->bindingCount; i++) {
        if (!trigger_equals(a->bindings[i].trigger, b->bindings[i].trigger) ||
            a->bindings[i].gesture != b->bindings[i].gesture || a->bindings[i].action != b->bindings[i].action) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Finds the action bound to a trigger and gesture.
 * @param config Configuration to search.
 * @param trigger Trigger to look for.
 * @param gesture Gesture to look for.
 * @param action Receives the action when found.
 * @return true when a binding exists.
 */
static bool find_binding(const Config *config, Trigger trigger, Gesture gesture, Action *action)
{
    for (int i = 0; i < config->bindingCount; i++) {
        if (config->bindings[i].gesture == gesture && trigger_equals(config->bindings[i].trigger, trigger)) {
            *action = config->bindings[i].action;
            return true;
        }
    }
    return false;
}

/** @brief Defaults match the documented values and placeholder bindings. */
static void test_defaults(void)
{
    Config config;
    Action action;
    config_defaults(&config);
    assert(config.doubleWindowMs == 300 && config.longPressMs == 600);
    assert(config.defaultTool == TOOL_PEN);
    assert(config.defaultStrokeIndex == 0 && config.defaultFillIndex == 0 && config.defaultWidthIndex == 1);
    assert(config.autoFadeSeconds == 0 && config.drawMonitor == 0 && config.uiMonitor == 0);
    assert(config.toolLock);
    assert(config.ui.autoHide && config.ui.autoHideMs == 1200 && config.ui.excludeFromCapture);
    for (int i = 0; i < PANEL_COUNT; i++) {
        assert(!config.ui.placement[i].set);
        assert(!config.ui.pinned[i]);
    }
    assert(config.bindingCount == 5);
    assert(find_binding(&config, key(VK_F13, 0), GESTURE_SINGLE, &action) && action == ACTION_UNDO);
    assert(find_binding(&config, key(VK_F13, 0), GESTURE_DOUBLE, &action) && action == ACTION_TOGGLE_OVERLAY);
    assert(find_binding(&config, key(VK_F14, 0), GESTURE_SINGLE, &action) && action == ACTION_NEXT_TOOL);
    assert(find_binding(&config, key(VK_F14, 0), GESTURE_DOUBLE, &action) && action == ACTION_TOGGLE_MODE);
    assert(find_binding(&config, key(VK_F14, 0), GESTURE_LONG, &action) && action == ACTION_CLEAR);
}

/** @brief Saving and loading reproduces every field, including placements and bindings. */
static void test_round_trip(void)
{
    wchar_t path[MAX_PATH];
    Config original;
    Config loaded;
    scratch_path(L"penpane_test_roundtrip.cfg", path, MAX_PATH);
    config_defaults(&original);
    original.doubleWindowMs = 250;
    original.longPressMs = 900;
    original.defaultTool = TOOL_LASER;
    original.defaultStrokeIndex = 3;
    original.defaultFillIndex = 2;
    original.defaultWidthIndex = 2;
    original.autoFadeSeconds = 12;
    original.drawMonitor = 1;
    original.uiMonitor = 2;
    original.toolLock = true;
    original.ui.autoHide = false;
    original.ui.autoHideMs = 1500;
    original.ui.excludeFromCapture = false;
    original.ui.placement[PANEL_TOOLBAR] = (PanelPlacement){true, -1920, 40};
    original.ui.placement[PANEL_HISTORY] = (PanelPlacement){true, 12, 900};
    original.ui.pinned[PANEL_TOOLBAR] = true;
    original.ui.pinned[PANEL_HISTORY] = true;
    Binding extra = {{TRIGGER_MOUSE_BUTTON, VK_XBUTTON1, 0}, GESTURE_SINGLE, ACTION_REDO};
    Binding modified = {key(VK_F15, TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT), GESTURE_LONG, ACTION_COPY_PNG};
    assert(config_set_binding(&original, extra));
    assert(config_set_binding(&original, modified));
    assert(config_save(&original, path));
    assert(config_load(&loaded, path, NULL, 0) == CONFIG_LOADED);
    assert(config_equal(&original, &loaded));
    DeleteFileW(path);
}

/** @brief Pin keys load, a reload replaces earlier values, and malformed values are rejected. */
static void test_pinned_keys(void)
{
    wchar_t path[MAX_PATH];
    wchar_t errors[256];
    Config config;
    scratch_path(L"penpane_test_pinned.cfg", path, MAX_PATH);
    write_text(path, "toolbar_pinned = true\nproperties_pinned = 1\nhistory_pinned = maybe\n");
    assert(config_load(&config, path, errors, 256) == CONFIG_LOADED);
    assert(config.ui.pinned[PANEL_TOOLBAR] && config.ui.pinned[PANEL_PROPERTIES]);
    assert(!config.ui.pinned[PANEL_HISTORY]);
    assert(errors[0] != L'\0');
    write_text(path, "history_pinned = yes\n");
    assert(config_load(&config, path, errors, 256) == CONFIG_LOADED);
    assert(!config.ui.pinned[PANEL_TOOLBAR] && !config.ui.pinned[PANEL_PROPERTIES]);
    assert(config.ui.pinned[PANEL_HISTORY]);
    DeleteFileW(path);
}

/** @brief Saving over an existing file replaces it and leaves no temporary file. */
static void test_save_replaces_file(void)
{
    wchar_t path[MAX_PATH];
    wchar_t temporary[MAX_PATH + 8];
    Config first;
    Config second;
    Config loaded;
    scratch_path(L"penpane_test_replace.cfg", path, MAX_PATH);
    config_defaults(&first);
    config_defaults(&second);
    second.autoFadeSeconds = 7;
    assert(config_save(&first, path));
    assert(config_save(&second, path));
    assert(config_load(&loaded, path, NULL, 0) == CONFIG_LOADED);
    assert(loaded.autoFadeSeconds == 7);
    wcscpy(temporary, path);
    wcscat(temporary, L".tmp");
    assert(GetFileAttributesW(temporary) == INVALID_FILE_ATTRIBUTES);
    DeleteFileW(path);
}

/** @brief A missing file reports failure and leaves defaults in place. */
static void test_missing_file(void)
{
    wchar_t path[MAX_PATH];
    wchar_t errors[128];
    Config config;
    Config defaults;
    scratch_path(L"penpane_test_does_not_exist.cfg", path, MAX_PATH);
    DeleteFileW(path);
    config_defaults(&defaults);
    config.doubleWindowMs = 1;
    assert(config_load(&config, path, errors, 128) == CONFIG_MISSING);
    assert(config_equal(&config, &defaults));
    assert(errors[0] == L'\0');
}

/** @brief Malformed and unknown lines are skipped and reported while valid lines still apply. */
static void test_bad_lines_skipped(void)
{
    wchar_t path[MAX_PATH];
    wchar_t errors[512];
    Config config;
    Action action;
    scratch_path(L"penpane_test_bad.cfg", path, MAX_PATH);
    write_text(path,
               "# comment\r\n"
               "\r\n"
               "double_press_ms = 250\r\n"
               "long_press_ms = banana\r\n"
               "unknown_key = 1\r\n"
               "no equals here\r\n"
               "default_tool = hexagon\r\n"
               "default_stroke_index = 99\r\n"
               "auto_fade_seconds = -3\r\n"
               "bind = F13 triple undo\r\n"
               "bind = F13 single nothing\r\n"
               "bind = Ctrl+ single undo\r\n"
               "bind = F13 single\r\n"
               "bind = F13 single undo extra\r\n"
               "toolbar_pos = 10;20\r\n"
               "tool_lock = maybe\r\n"
               "default_tool = ellipse\r\n"
               "bind = F16 double redo\r\n"
               "properties_pos = 5, 6\r\n");
    assert(config_load(&config, path, errors, 512) == CONFIG_LOADED);
    assert(config.doubleWindowMs == 250);
    assert(config.longPressMs == 600);
    assert(config.defaultTool == TOOL_ELLIPSE);
    assert(config.defaultStrokeIndex == 0);
    assert(config.autoFadeSeconds == 0);
    assert(config.toolLock);
    assert(!config.ui.placement[PANEL_TOOLBAR].set);
    assert(config.ui.placement[PANEL_PROPERTIES].set && config.ui.placement[PANEL_PROPERTIES].x == 5 &&
           config.ui.placement[PANEL_PROPERTIES].y == 6);
    assert(config.bindingCount == 6);
    assert(find_binding(&config, key(VK_F16, 0), GESTURE_DOUBLE, &action) && action == ACTION_REDO);
    assert(find_binding(&config, key(VK_F13, 0), GESTURE_SINGLE, &action) && action == ACTION_UNDO);
    assert(wcsstr(errors, L"line 4:") != NULL);
    assert(wcsstr(errors, L"line 5:") != NULL);
    assert(wcsstr(errors, L"line 6:") != NULL);
    assert(wcsstr(errors, L"line 3:") == NULL);
    assert(wcsstr(errors, L"line 18:") == NULL);
    DeleteFileW(path);
}

/** @brief A tiny error buffer is never overrun and stays terminated. */
static void test_error_buffer_bounds(void)
{
    wchar_t path[MAX_PATH];
    wchar_t errors[16];
    Config config;
    scratch_path(L"penpane_test_small_errors.cfg", path, MAX_PATH);
    write_text(path, "a = 1\nb = 2\nc = 3\nd = 4\n");
    assert(config_load(&config, path, errors, 16) == CONFIG_LOADED);
    assert(wcslen(errors) < 16);
    assert(config_load(&config, path, NULL, 0) == CONFIG_LOADED);
    DeleteFileW(path);
}

/** @brief Files without reset_bindings layer over the defaults; with it they replace them. */
static void test_reset_bindings(void)
{
    wchar_t path[MAX_PATH];
    Config config;
    Action action;
    scratch_path(L"penpane_test_reset.cfg", path, MAX_PATH);
    write_text(path, "bind = F13 single redo\n");
    assert(config_load(&config, path, NULL, 0) == CONFIG_LOADED);
    assert(config.bindingCount == 5);
    assert(find_binding(&config, key(VK_F13, 0), GESTURE_SINGLE, &action) && action == ACTION_REDO);

    write_text(path, "reset_bindings = true\nbind = F20 long clear\n");
    assert(config_load(&config, path, NULL, 0) == CONFIG_LOADED);
    assert(config.bindingCount == 1);
    assert(find_binding(&config, key(VK_F20, 0), GESTURE_LONG, &action) && action == ACTION_CLEAR);
    DeleteFileW(path);
}

/** @brief A byte-order mark, comments and surrounding blanks are tolerated. */
static void test_bom_and_spacing(void)
{
    wchar_t path[MAX_PATH];
    Config config;
    scratch_path(L"penpane_test_bom.cfg", path, MAX_PATH);
    write_text(path, "\xEF\xBB\xBF  double_press_ms=  400  \n\t# note\nLONG_PRESS_MS = 700");
    assert(config_load(&config, path, NULL, 0) == CONFIG_LOADED);
    assert(config.doubleWindowMs == 400);
    assert(config.longPressMs == 700);
    DeleteFileW(path);
}

/** @brief set_binding replaces an existing trigger and gesture, appends otherwise, and reports a full table. */
static void test_set_binding(void)
{
    Config config;
    Action action;
    config_defaults(&config);
    Binding override = {key(VK_F13, 0), GESTURE_SINGLE, ACTION_REDO};
    assert(config_set_binding(&config, override));
    assert(config.bindingCount == 5);
    assert(find_binding(&config, key(VK_F13, 0), GESTURE_SINGLE, &action) && action == ACTION_REDO);

    Binding added = {key(VK_F13, 0), GESTURE_LONG, ACTION_COPY_PNG};
    assert(config_set_binding(&config, added));
    assert(config.bindingCount == 6);

    while (config.bindingCount < CONFIG_MAX_BINDINGS) {
        unsigned number = (unsigned)config.bindingCount;
        Binding filler = {key(0x41 + number % 26u, TRIGGER_MOD_CONTROL | (number / 26u) * TRIGGER_MOD_SHIFT),
                          GESTURE_SINGLE, ACTION_UNDO};
        assert(config_set_binding(&config, filler));
    }
    Binding overflow = {key(0x42, TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT), GESTURE_SINGLE, ACTION_UNDO};
    assert(!config_set_binding(&config, overflow));
    assert(config_set_binding(&config, override));
}

/** @brief clear_action removes every binding of one action and keeps the order of the rest. */
static void test_clear_action(void)
{
    Config config;
    Config loaded;
    wchar_t path[MAX_PATH];
    Action action;
    config_defaults(&config);
    Binding second = {key(VK_F20, 0), GESTURE_SINGLE, ACTION_UNDO};
    assert(config_set_binding(&config, second));
    config_clear_action(&config, ACTION_UNDO);
    assert(config.bindingCount == 4);
    assert(!find_binding(&config, key(VK_F13, 0), GESTURE_SINGLE, &action));
    assert(!find_binding(&config, key(VK_F20, 0), GESTURE_SINGLE, &action));
    assert(config.bindings[0].action == ACTION_TOGGLE_OVERLAY && config.bindings[3].action == ACTION_CLEAR);

    scratch_path(L"penpane_test_clear.cfg", path, MAX_PATH);
    assert(config_save(&config, path));
    assert(config_load(&loaded, path, NULL, 0) == CONFIG_LOADED);
    assert(config_equal(&config, &loaded));
    config_clear_action(&config, ACTION_CLEAR);
    config_clear_action(&config, ACTION_NEXT_TOOL);
    config_clear_action(&config, ACTION_TOGGLE_MODE);
    config_clear_action(&config, ACTION_TOGGLE_OVERLAY);
    assert(config.bindingCount == 0);
    assert(config_save(&config, path));
    assert(config_load(&loaded, path, NULL, 0) == CONFIG_LOADED);
    assert(loaded.bindingCount == 0);
    DeleteFileW(path);
}

/**
 * @brief Formats a trigger, parses it back and checks equality.
 * @param trigger Trigger to round-trip.
 * @param expected Expected text, or NULL to skip the text check.
 */
static void check_trigger_round_trip(Trigger trigger, const wchar_t *expected)
{
    wchar_t text[64];
    Trigger parsed;
    trigger_to_text(trigger, text, 64);
    if (expected != NULL) {
        assert(wcscmp(text, expected) == 0);
    }
    assert(trigger_from_text(text, &parsed));
    assert(trigger_equals(trigger, parsed));
}

/** @brief Trigger text formats canonically and parses back for every key class. */
static void test_trigger_text_round_trip(void)
{
    for (unsigned vk = 'A'; vk <= 'Z'; vk++) {
        check_trigger_round_trip(key(vk, 0), NULL);
    }
    for (unsigned vk = '0'; vk <= '9'; vk++) {
        check_trigger_round_trip(key(vk, 0), NULL);
    }
    for (unsigned vk = VK_F1; vk <= VK_F24; vk++) {
        check_trigger_round_trip(key(vk, TRIGGER_MOD_SHIFT), NULL);
    }
    for (unsigned vk = VK_NUMPAD0; vk <= VK_NUMPAD9; vk++) {
        check_trigger_round_trip(key(vk, 0), NULL);
    }
    check_trigger_round_trip(key(VK_F13, 0), L"F13");
    check_trigger_round_trip(key(VK_F14, TRIGGER_MOD_CONTROL), L"Ctrl+F14");
    check_trigger_round_trip(key(VK_F14, TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT | TRIGGER_MOD_SHIFT | TRIGGER_MOD_WIN),
                             L"Ctrl+Alt+Shift+Win+F14");
    check_trigger_round_trip(key(VK_ESCAPE, 0), L"Esc");
    check_trigger_round_trip(key(VK_NEXT, 0), L"PageDown");
    check_trigger_round_trip(key(VK_PRIOR, 0), L"PageUp");
    check_trigger_round_trip(key(VK_OEM_3, 0), L"VK_0xC0");
    check_trigger_round_trip(key(0x07, TRIGGER_MOD_ALT), L"Alt+VK_0x07");
    Trigger middle = {TRIGGER_MOUSE_BUTTON, VK_MBUTTON, 0};
    Trigger x1 = {TRIGGER_MOUSE_BUTTON, VK_XBUTTON1, 0};
    Trigger x2 = {TRIGGER_MOUSE_BUTTON, VK_XBUTTON2, TRIGGER_MOD_CONTROL};
    check_trigger_round_trip(middle, L"MButton");
    check_trigger_round_trip(x1, L"XButton1");
    check_trigger_round_trip(x2, L"Ctrl+XButton2");
}

/** @brief Parsing is case-insensitive, tolerates blanks and rejects malformed text. */
static void test_trigger_parsing(void)
{
    Trigger trigger;
    assert(trigger_from_text(L"ctrl + alt + f13", &trigger));
    assert(trigger.kind == TRIGGER_KEY && trigger.virtualKey == VK_F13 &&
           trigger.modifiers == (TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT));
    assert(trigger_from_text(L"xbutton2", &trigger) && trigger.kind == TRIGGER_MOUSE_BUTTON &&
           trigger.virtualKey == VK_XBUTTON2);
    assert(trigger_from_text(L"vk_0x7c", &trigger) && trigger.virtualKey == 0x7C);
    assert(trigger_from_text(L"a", &trigger) && trigger.virtualKey == 'A');
    assert(trigger_from_text(L"Control+Z", &trigger) && trigger.modifiers == TRIGGER_MOD_CONTROL);

    const wchar_t *bad[] = {L"",         L"Ctrl+",     L"+F1",       L"Ctrl++F1",  L"Foo",
                            L"F0",       L"F25",       L"F-1",       L"VK_0x",     L"VK_0x1FF",
                            L"VK_0x00",  L"VK_0xZZ",   L"Hyper+F1",  L"AB",        L"Numpad10",
                            L"Ctrl+Foo", L"F13 F14",   L"Ctrl Alt+F1"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        assert(!trigger_from_text(bad[i], &trigger));
    }
    assert(!trigger_from_text(NULL, &trigger));
    assert(!trigger_from_text(L"é", &trigger));
}

/** @brief Output buffers that are too small are truncated and terminated. */
static void test_trigger_to_text_truncation(void)
{
    wchar_t text[6];
    trigger_to_text(key(VK_F14, TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT), text, 6);
    assert(wcslen(text) == 5);
    trigger_to_text(key(VK_F14, 0), text, 1);
    assert(text[0] == L'\0');
}

/** @brief A file that cannot be opened for a reason other than absence is unreadable and leaves the configuration untouched. */
static void test_unreadable_file_is_not_missing(void)
{
    wchar_t path[MAX_PATH];
    wchar_t folder[MAX_PATH];
    Config config;
    scratch_path(L"penpane_test_locked.cfg", path, MAX_PATH);
    write_text(path, "double_press_ms = 111\n");
    HANDLE lock = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(lock != INVALID_HANDLE_VALUE);
    config_defaults(&config);
    config.doubleWindowMs = 77;
    config.bindingCount = 2;
    assert(config_load(&config, path, NULL, 0) == CONFIG_UNREADABLE);
    assert(config.doubleWindowMs == 77 && config.bindingCount == 2);
    CloseHandle(lock);
    assert(config_load(&config, path, NULL, 0) == CONFIG_LOADED);
    assert(config.doubleWindowMs == 111);
    DeleteFileW(path);

    scratch_path(L"penpane_test_folder.cfg", folder, MAX_PATH);
    RemoveDirectoryW(folder);
    assert(CreateDirectoryW(folder, NULL));
    config.doubleWindowMs = 78;
    assert(config_load(&config, folder, NULL, 0) == CONFIG_UNREADABLE);
    assert(config.doubleWindowMs == 78);
    RemoveDirectoryW(folder);

    scratch_path(L"penpane_test_no_such_folder\\penpane.ini", path, MAX_PATH);
    config.doubleWindowMs = 79;
    assert(config_load(&config, path, NULL, 0) == CONFIG_UNREADABLE);
    assert(config.doubleWindowMs == 79);
}

/** @brief Plain typing keys need Ctrl, Alt or Win; Shift alone does not count; other keys and mouse buttons are free. */
static void test_bindable_triggers(void)
{
    for (unsigned vk = 'A'; vk <= 'Z'; vk++) {
        assert(!trigger_is_bindable(key(vk, 0)));
        assert(!trigger_is_bindable(key(vk, TRIGGER_MOD_SHIFT)));
        assert(trigger_is_bindable(key(vk, TRIGGER_MOD_CONTROL)));
        assert(trigger_is_bindable(key(vk, TRIGGER_MOD_ALT | TRIGGER_MOD_SHIFT)));
        assert(trigger_is_bindable(key(vk, TRIGGER_MOD_WIN)));
    }
    for (unsigned vk = '0'; vk <= '9'; vk++) {
        assert(!trigger_is_bindable(key(vk, 0)));
    }
    assert(!trigger_is_bindable(key(VK_ESCAPE, 0)));
    assert(!trigger_is_bindable(key(VK_RETURN, 0)));
    assert(!trigger_is_bindable(key(VK_TAB, 0)));
    assert(!trigger_is_bindable(key(VK_SPACE, TRIGGER_MOD_SHIFT)));
    assert(!trigger_is_bindable(key(VK_BACK, 0)));
    assert(!trigger_is_bindable(key(VK_OEM_COMMA, 0)));
    assert(!trigger_is_bindable(key(VK_OEM_3, 0)));
    assert(trigger_is_bindable(key(VK_ESCAPE, TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT | TRIGGER_MOD_SHIFT)));
    assert(trigger_is_bindable(key(VK_F13, 0)));
    assert(trigger_is_bindable(key(VK_F1, TRIGGER_MOD_SHIFT)));
    assert(trigger_is_bindable(key(VK_NUMPAD0, 0)));
    assert(trigger_is_bindable(key(VK_LEFT, 0)));
    Trigger middle = {TRIGGER_MOUSE_BUTTON, VK_MBUTTON, 0};
    assert(trigger_is_bindable(middle));
}

/** @brief Plain typing keys are rejected from the file and from set_binding, with a reported line, and never stored. */
static void test_plain_keys_rejected(void)
{
    wchar_t path[MAX_PATH];
    wchar_t errors[512];
    Config config;
    Action action;
    scratch_path(L"penpane_test_plain.cfg", path, MAX_PATH);
    write_text(path,
               "reset_bindings = true\n"
               "bind = A single undo\n"
               "bind = Shift+B single redo\n"
               "bind = Esc double clear\n"
               "bind = Space long clear\n"
               "bind = 5 single clear\n"
               "bind = Ctrl+A single undo\n"
               "bind = Win+Esc single redo\n"
               "bind = F15 single next_tool\n");
    assert(config_load(&config, path, errors, 512) == CONFIG_LOADED);
    assert(config.bindingCount == 3);
    assert(!find_binding(&config, key('A', 0), GESTURE_SINGLE, &action));
    assert(!find_binding(&config, key(VK_ESCAPE, 0), GESTURE_DOUBLE, &action));
    assert(find_binding(&config, key('A', TRIGGER_MOD_CONTROL), GESTURE_SINGLE, &action));
    assert(find_binding(&config, key(VK_F15, 0), GESTURE_SINGLE, &action));
    assert(wcsstr(errors, L"line 2:") != NULL && wcsstr(errors, L"line 3:") != NULL);
    assert(wcsstr(errors, L"line 4:") != NULL && wcsstr(errors, L"line 5:") != NULL);
    assert(wcsstr(errors, L"line 6:") != NULL);
    assert(wcsstr(errors, L"line 7:") == NULL && wcsstr(errors, L"line 9:") == NULL);
    DeleteFileW(path);

    config_defaults(&config);
    int before = config.bindingCount;
    Binding plain = {key('Q', 0), GESTURE_SINGLE, ACTION_UNDO};
    assert(!config_set_binding(&config, plain));
    assert(config.bindingCount == before);
    plain.trigger = key('Q', TRIGGER_MOD_ALT);
    assert(config_set_binding(&config, plain));
    assert(config.bindingCount == before + 1);
}

/** @brief The distinct trigger count ignores repeated triggers and distinguishes modifiers. */
static void test_trigger_count(void)
{
    Config config;
    config_defaults(&config);
    assert(config_trigger_count(&config) == 2);
    Binding extra = {key(VK_F13, TRIGGER_MOD_CONTROL), GESTURE_SINGLE, ACTION_UNDO};
    assert(config_set_binding(&config, extra));
    assert(config_trigger_count(&config) == 3);
    extra.gesture = GESTURE_LONG;
    assert(config_set_binding(&config, extra));
    assert(config_trigger_count(&config) == 3);
    config.bindingCount = 0;
    assert(config_trigger_count(&config) == 0);
}

/** @brief A legacy file is copied once to the current name; an existing current file or a missing legacy file changes nothing. */
static void test_legacy_migration(void)
{
    wchar_t legacy[MAX_PATH];
    wchar_t current[MAX_PATH];
    Config config;
    scratch_path(L"penpane_test_legacy.cfg", legacy, MAX_PATH);
    scratch_path(L"penpane_test_current.cfg", current, MAX_PATH);
    DeleteFileW(legacy);
    DeleteFileW(current);
    assert(!config_migrate_legacy(legacy, current));
    assert(GetFileAttributesW(current) == INVALID_FILE_ATTRIBUTES);

    write_text(legacy, "double_press_ms = 321\n");
    assert(config_migrate_legacy(legacy, current));
    assert(GetFileAttributesW(legacy) != INVALID_FILE_ATTRIBUTES);
    assert(config_load(&config, current, NULL, 0) == CONFIG_LOADED);
    assert(config.doubleWindowMs == 321);

    write_text(legacy, "double_press_ms = 123\n");
    assert(!config_migrate_legacy(legacy, current));
    assert(config_load(&config, current, NULL, 0) == CONFIG_LOADED);
    assert(config.doubleWindowMs == 321);
    DeleteFileW(legacy);
    DeleteFileW(current);
}

/**
 * @brief Runs every configuration test.
 * @return 0 when all assertions hold.
 */
int main(void)
{
    test_unreadable_file_is_not_missing();
    test_bindable_triggers();
    test_plain_keys_rejected();
    test_trigger_count();
    test_defaults();
    test_round_trip();
    test_pinned_keys();
    test_save_replaces_file();
    test_missing_file();
    test_bad_lines_skipped();
    test_error_buffer_bounds();
    test_reset_bindings();
    test_bom_and_spacing();
    test_set_binding();
    test_clear_action();
    test_trigger_text_round_trip();
    test_trigger_parsing();
    test_trigger_to_text_truncation();
    test_legacy_migration();
    printf("test_config: 18 tests passed\n");
    return 0;
}
