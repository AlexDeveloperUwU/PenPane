/**
 * @file gesture.h
 * @brief Platform-independent resolver that turns key/button presses into single, double and long gestures.
 *
 * The engine knows nothing about Windows. Callers feed it press and release events with a
 * millisecond clock and ask it when it next needs to be ticked, so no timer runs while idle.
 *
 * Rules:
 *  - A trigger with only a single-press binding fires immediately on press.
 *  - A trigger with a long binding fires the long action when the press has lasted longTimeMs,
 *    while still held. A shorter press counts as a single (or the first half of a double).
 *  - A trigger with a double binding delays its single action by doubleWindowMs after release,
 *    so that a second press inside the window fires the double action instead.
 */
#ifndef INK_GESTURE_H
#define INK_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "actions.h"

#define GESTURE_MAX_BINDINGS 64
#define GESTURE_MAX_TRACKED 16

/** @brief Source of a trigger. */
typedef enum TriggerKind {
    TRIGGER_KEY,
    TRIGGER_MOUSE_BUTTON
} TriggerKind;

/** @brief Modifier bits stored in Trigger.modifiers, same values as the Win32 MOD_* flags. */
#define TRIGGER_MOD_ALT 0x1u
#define TRIGGER_MOD_CONTROL 0x2u
#define TRIGGER_MOD_SHIFT 0x4u
#define TRIGGER_MOD_WIN 0x8u

/** @brief A physical key or mouse button, optionally with modifiers. */
typedef struct Trigger {
    TriggerKind kind;
    unsigned virtualKey;
    unsigned modifiers;
} Trigger;

/** @brief How a trigger is pressed. */
typedef enum Gesture {
    GESTURE_SINGLE,
    GESTURE_DOUBLE,
    GESTURE_LONG
} Gesture;

/** @brief One assignment of an action to a trigger and gesture. */
typedef struct Binding {
    Trigger trigger;
    Gesture gesture;
    Action action;
} Binding;

/**
 * @brief Receives resolved actions.
 * @param user Opaque pointer given to gesture_init.
 * @param action Resolved action.
 */
typedef void (*GestureSink)(void *user, Action action);

/** @brief Per-trigger tracking state; internal to the engine but declared for static allocation. */
typedef struct GestureTracked {
    Trigger trigger;
    bool inUse;
    bool down;
    bool longFired;
    bool pendingSingle;
    uint32_t pressedAt;
    uint32_t releasedAt;
    int pressCount;
    bool hasSingle;
    bool hasDouble;
    bool hasLong;
    Action singleAction;
    Action doubleAction;
    Action longAction;
} GestureTracked;

/** @brief Gesture resolver state. */
typedef struct GestureEngine {
    GestureTracked tracked[GESTURE_MAX_TRACKED];
    int doubleWindowMs;
    int longPressMs;
    GestureSink sink;
    void *user;
} GestureEngine;

/**
 * @brief Initializes the engine with a binding table.
 * @param engine Engine to initialize.
 * @param bindings Binding table; later entries for the same trigger and gesture override earlier ones.
 * @param count Number of bindings, at most GESTURE_MAX_BINDINGS.
 * @param doubleWindowMs Maximum gap between presses of a double gesture.
 * @param longPressMs Minimum press duration of a long gesture.
 * @param sink Receives resolved actions.
 * @param user Opaque pointer forwarded to the sink.
 */
void gesture_init(GestureEngine *engine, const Binding *bindings, int count, int doubleWindowMs,
                  int longPressMs, GestureSink sink, void *user);

/**
 * @brief Reports whether a trigger has any binding.
 * @param engine Engine to query.
 * @param trigger Trigger to look up.
 * @return true when at least one gesture is bound to the trigger.
 */
bool gesture_is_bound(const GestureEngine *engine, Trigger trigger);

/**
 * @brief Reports whether a trigger needs release events (it has a long or double binding).
 * @param engine Engine to query.
 * @param trigger Trigger to look up.
 * @return true when the caller must report releases for this trigger.
 */
bool gesture_needs_release(const GestureEngine *engine, Trigger trigger);

/**
 * @brief Reports a press. Auto-repeat presses while already down are ignored.
 * @param engine Engine to update.
 * @param trigger Pressed trigger.
 * @param nowMs Monotonic millisecond clock.
 */
void gesture_press(GestureEngine *engine, Trigger trigger, uint32_t nowMs);

/**
 * @brief Reports a release.
 * @param engine Engine to update.
 * @param trigger Released trigger.
 * @param nowMs Monotonic millisecond clock.
 */
void gesture_release(GestureEngine *engine, Trigger trigger, uint32_t nowMs);

/**
 * @brief Fires any gesture whose deadline has passed.
 * @param engine Engine to update.
 * @param nowMs Monotonic millisecond clock.
 * @return Milliseconds until the next deadline, or -1 when nothing is pending and no timer is needed.
 */
int gesture_tick(GestureEngine *engine, uint32_t nowMs);

/**
 * @brief Compares two triggers.
 * @param a First trigger.
 * @param b Second trigger.
 * @return true when kind, key and modifiers all match.
 */
bool trigger_equals(Trigger a, Trigger b);

#endif
