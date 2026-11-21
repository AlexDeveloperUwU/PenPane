/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <windows.h>
#include <imm.h>
#include <stdbool.h>

#include "app.h"

#define KEYBOARD_LAYOUT_LIMIT 64

/**
 * @brief Tells whether a keyboard layout is a plain layout of a Latin-script language that needs no text services.
 * @param layout Keyboard layout handle.
 * @return true for an ordinary layout of a supported language.
 */
static bool is_plain_layout(HKL layout)
{
    static const WORD LANGUAGES[] = {LANG_ENGLISH, LANG_SPANISH, LANG_CATALAN, LANG_GALICIAN,
                                     LANG_BASQUE,  LANG_FRENCH,  LANG_GERMAN,  LANG_ITALIAN,
                                     LANG_PORTUGUESE, LANG_DUTCH};
    if (((UINT_PTR)layout & 0xF0000000u) == 0xE0000000u) {
        return false;
    }
    WORD primary = PRIMARYLANGID(LOWORD((UINT_PTR)layout));
    for (size_t i = 0; i < sizeof LANGUAGES / sizeof LANGUAGES[0]; i++) {
        if (primary == LANGUAGES[i]) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Tells whether text services must stay on because an installed layout may need composition input.
 * @return false only when every installed keyboard layout is a plain layout.
 */
static bool needs_text_services(void)
{
    HKL layouts[KEYBOARD_LAYOUT_LIMIT];
    int count = GetKeyboardLayoutList(KEYBOARD_LAYOUT_LIMIT, layouts);
    if (count <= 0) {
        return true;
    }
    for (int i = 0; i < count; i++) {
        if (!is_plain_layout(layouts[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Program entry point: configures text services and process DPI awareness, then runs the application.
 * @param instance Module instance of the executable.
 * @param previous Always NULL on current Windows versions, unused.
 * @param command Command line without the program name, unused.
 * @param show Initial show state, unused because the application starts hidden.
 * @return Process exit code.
 */
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show)
{
    (void)previous;
    (void)command;
    (void)show;
    if (!needs_text_services()) {
        ImmDisableIME((DWORD)-1);
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return app_run(instance);
}
