/**
 * @file hotkeys.h
 * @brief Global hotkeys, mouse-button triggers and learn mode on top of the gesture engine.
 *
 * Keyboard triggers use RegisterHotKey (no global hook, no cost while idle, other applications'
 * own hotkeys are unaffected). RegisterHotKey reports presses only, so releases of triggers that
 * have long or double bindings are detected with a short polling timer that runs only while such
 * a trigger is held. Mouse-button triggers use a low-level mouse hook installed only when at
 * least one mouse binding exists. Learn mode installs a low-level keyboard and mouse hook only
 * for its duration.
 *
 * The emergency exit (Ctrl+Alt+Shift+Esc) is registered separately by the owner of the message
 * window and is not part of the binding table.
 */
#ifndef INK_HOTKEYS_H
#define INK_HOTKEYS_H

#include <stdbool.h>
#include <windows.h>

#include "config.h"

/**
 * @brief Receives resolved actions.
 * @param user Opaque pointer given to hotkeys_create.
 * @param action Resolved action.
 */
typedef void (*HotkeyActionHandler)(void *user, Action action);

/** @brief How learn mode ended. */
typedef enum LearnOutcome {
    LEARN_CAPTURED,
    LEARN_CANCELLED,
    LEARN_REJECTED
} LearnOutcome;

/**
 * @brief Receives the result of learn mode.
 * @param user Opaque pointer given to hotkeys_create.
 * @param action Action that was being learned.
 * @param binding Captured trigger and gesture; meaningful only when outcome is LEARN_CAPTURED.
 * @param outcome LEARN_CAPTURED, LEARN_CANCELLED (escape, timeout, resume) or LEARN_REJECTED (a plain typing key that needs Ctrl, Alt or Win).
 */
typedef void (*HotkeyLearnHandler)(void *user, Action action, Binding binding, LearnOutcome outcome);

/** @brief Opaque instance. */
typedef struct Hotkeys Hotkeys;

/**
 * @brief Base message id of the module timers; the owner forwards every WM_TIMER to hotkeys_handle_message.
 *
 * Ids HOTKEYS_TIMER_ID to HOTKEYS_TIMER_ID + 2 are reserved. Registered hotkey ids start at 0x4C00.
 * Only one instance can exist at a time because the low-level hooks are process-wide.
 */
#define HOTKEYS_TIMER_ID 0x4B48

/**
 * @brief Creates an instance bound to a message window.
 * @param window Window that receives WM_HOTKEY and WM_TIMER; messages must be forwarded to hotkeys_handle_message.
 * @param onAction Receives resolved actions.
 * @param onLearn Receives learn-mode results.
 * @param user Opaque pointer forwarded to both handlers.
 * @return New instance, or NULL on failure.
 */
Hotkeys *hotkeys_create(HWND window, HotkeyActionHandler onAction, HotkeyLearnHandler onLearn,
                        void *user);

/**
 * @brief Unregisters everything and frees the instance.
 * @param hotkeys Instance to destroy, may be NULL.
 */
void hotkeys_destroy(Hotkeys *hotkeys);

/**
 * @brief Replaces the binding table and re-registers hotkeys.
 * @param hotkeys Instance to configure.
 * @param config Source of bindings and timings.
 * @param report Optional buffer receiving one line per trigger that could not be registered (for example because another application owns it), or an empty string.
 * @param reportSize Size of the report buffer in characters.
 * @return true when every trigger registered, false when at least one failed (the rest still work).
 */
bool hotkeys_apply(Hotkeys *hotkeys, const Config *config, wchar_t *report, int reportSize);

/**
 * @brief Handles WM_HOTKEY and WM_TIMER messages meant for this module.
 * @param hotkeys Instance.
 * @param message Window message id.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return true when the message belonged to this module.
 */
bool hotkeys_handle_message(Hotkeys *hotkeys, UINT message, WPARAM wparam, LPARAM lparam);

/**
 * @brief Starts learn mode: the next key or mouse button gesture the user performs is captured.
 *
 * While learning, normal hotkeys are suspended and the captured input is swallowed. The gesture
 * is classified like in normal use: held at least longPressMs is long, two presses within
 * doubleWindowMs is double, otherwise single. Pressing Escape alone cancels. Times out after
 * 15 seconds.
 * @param hotkeys Instance.
 * @param action Action to bind to the captured gesture.
 * @return false when learn mode is already running.
 */
bool hotkeys_learn_begin(Hotkeys *hotkeys, Action action);

/**
 * @brief Cancels learn mode if it is running; the learn handler is called with LEARN_CANCELLED.
 * @param hotkeys Instance.
 */
void hotkeys_learn_cancel(Hotkeys *hotkeys);

/**
 * @brief Recovers from a period in which input may have been missed, such as sleep or a locked session.
 *
 * Releases every mouse trigger the engine still considers held, drops queued events, cancels learn
 * mode and installs the low-level hooks again, because the system removes a hook silently when it
 * does not answer in time.
 * @param hotkeys Instance, may be NULL.
 */
void hotkeys_reset_hooks(Hotkeys *hotkeys);

#endif
