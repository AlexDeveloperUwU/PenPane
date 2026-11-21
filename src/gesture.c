/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gesture.h"

#include <string.h>

/**
 * @brief Computes the milliseconds elapsed since a timestamp, wrap-around safe.
 * @param nowMs Current clock value.
 * @param thenMs Earlier clock value.
 * @return Signed elapsed time; negative when thenMs is in the future.
 */
static int32_t elapsed_ms(uint32_t nowMs, uint32_t thenMs)
{
    return (int32_t)(nowMs - thenMs);
}

/**
 * @brief Delivers an action to the sink if one is set.
 * @param engine Engine owning the sink.
 * @param action Action to deliver.
 */
static void emit(GestureEngine *engine, Action action)
{
    if (engine->sink != NULL) {
        engine->sink(engine->user, action);
    }
}

/**
 * @brief Finds the tracking slot of a trigger.
 * @param engine Engine to search.
 * @param trigger Trigger to look up.
 * @return Slot, or NULL when the trigger has no binding.
 */
static GestureTracked *find_tracked(const GestureEngine *engine, Trigger trigger)
{
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        const GestureTracked *slot = &engine->tracked[i];
        if (slot->inUse && trigger_equals(slot->trigger, trigger)) {
            return (GestureTracked *)slot;
        }
    }
    return NULL;
}

/**
 * @brief Finds the slot of a trigger, claiming a free one when needed.
 * @param engine Engine to modify.
 * @param trigger Trigger to look up.
 * @return Slot, or NULL when the table is full.
 */
static GestureTracked *claim_tracked(GestureEngine *engine, Trigger trigger)
{
    GestureTracked *existing = find_tracked(engine, trigger);
    if (existing != NULL) {
        return existing;
    }
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        if (!engine->tracked[i].inUse) {
            memset(&engine->tracked[i], 0, sizeof engine->tracked[i]);
            engine->tracked[i].inUse = true;
            engine->tracked[i].trigger = trigger;
            return &engine->tracked[i];
        }
    }
    return NULL;
}

/**
 * @brief Fires whatever deadline of one trigger has passed: the long press or the expiry of a double window.
 * @param engine Engine owning the slot.
 * @param slot Slot to advance.
 * @param nowMs Current clock value.
 */
static void advance(GestureEngine *engine, GestureTracked *slot, uint32_t nowMs)
{
    if (slot->down && slot->hasLong && !slot->longFired &&
        elapsed_ms(nowMs, slot->pressedAt) >= engine->longPressMs) {
        slot->longFired = true;
        slot->pendingSingle = false;
        emit(engine, slot->longAction);
    }
    if (slot->pendingSingle && !slot->down &&
        elapsed_ms(nowMs, slot->releasedAt) >= engine->doubleWindowMs) {
        slot->pendingSingle = false;
        slot->pressCount = 0;
        if (slot->hasSingle) {
            emit(engine, slot->singleAction);
        }
    }
}

/**
 * @brief Computes the time until the next deadline of one trigger.
 * @param engine Engine owning the slot.
 * @param slot Slot to inspect.
 * @param nowMs Current clock value.
 * @return Milliseconds remaining (at least 1), or -1 when the slot has no deadline.
 */
static int slot_deadline(const GestureEngine *engine, const GestureTracked *slot, uint32_t nowMs)
{
    int remaining = -1;
    if (slot->down && slot->hasLong && !slot->longFired) {
        remaining = engine->longPressMs - elapsed_ms(nowMs, slot->pressedAt);
    } else if (slot->pendingSingle && !slot->down) {
        remaining = engine->doubleWindowMs - elapsed_ms(nowMs, slot->releasedAt);
    } else {
        return -1;
    }
    return remaining < 1 ? 1 : remaining;
}

bool trigger_equals(Trigger a, Trigger b)
{
    return a.kind == b.kind && a.virtualKey == b.virtualKey && a.modifiers == b.modifiers;
}

void gesture_init(GestureEngine *engine, const Binding *bindings, int count, int doubleWindowMs,
                  int longPressMs, GestureSink sink, void *user)
{
    memset(engine, 0, sizeof *engine);
    engine->doubleWindowMs = doubleWindowMs;
    engine->longPressMs = longPressMs;
    engine->sink = sink;
    engine->user = user;
    if (bindings == NULL) {
        return;
    }
    if (count > GESTURE_MAX_BINDINGS) {
        count = GESTURE_MAX_BINDINGS;
    }
    for (int i = 0; i < count; i++) {
        GestureTracked *slot = claim_tracked(engine, bindings[i].trigger);
        if (slot == NULL) {
            continue;
        }
        switch (bindings[i].gesture) {
        case GESTURE_SINGLE:
            slot->hasSingle = true;
            slot->singleAction = bindings[i].action;
            break;
        case GESTURE_DOUBLE:
            slot->hasDouble = true;
            slot->doubleAction = bindings[i].action;
            break;
        case GESTURE_LONG:
            slot->hasLong = true;
            slot->longAction = bindings[i].action;
            break;
        }
    }
}

bool gesture_is_bound(const GestureEngine *engine, Trigger trigger)
{
    return find_tracked(engine, trigger) != NULL;
}

bool gesture_needs_release(const GestureEngine *engine, Trigger trigger)
{
    const GestureTracked *slot = find_tracked(engine, trigger);
    return slot != NULL && (slot->hasLong || slot->hasDouble);
}

void gesture_press(GestureEngine *engine, Trigger trigger, uint32_t nowMs)
{
    GestureTracked *slot = find_tracked(engine, trigger);
    if (slot == NULL || slot->down) {
        return;
    }
    advance(engine, slot, nowMs);
    slot->down = true;
    slot->pressedAt = nowMs;
    slot->longFired = false;
    if (slot->pendingSingle) {
        slot->pendingSingle = false;
        slot->pressCount = 0;
        slot->longFired = true;
        emit(engine, slot->doubleAction);
        return;
    }
    if (!slot->hasLong && !slot->hasDouble) {
        slot->longFired = true;
        if (slot->hasSingle) {
            emit(engine, slot->singleAction);
        }
    }
}

void gesture_release(GestureEngine *engine, Trigger trigger, uint32_t nowMs)
{
    GestureTracked *slot = find_tracked(engine, trigger);
    if (slot == NULL || !slot->down) {
        return;
    }
    advance(engine, slot, nowMs);
    slot->down = false;
    if (slot->longFired) {
        slot->longFired = false;
        slot->pressCount = 0;
        return;
    }
    if (slot->hasDouble) {
        slot->pendingSingle = true;
        slot->pressCount = 1;
        slot->releasedAt = nowMs;
        return;
    }
    if (slot->hasSingle) {
        emit(engine, slot->singleAction);
    }
}

int gesture_tick(GestureEngine *engine, uint32_t nowMs)
{
    int next = -1;
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        GestureTracked *slot = &engine->tracked[i];
        if (!slot->inUse) {
            continue;
        }
        advance(engine, slot, nowMs);
        int remaining = slot_deadline(engine, slot, nowMs);
        if (remaining >= 0 && (next < 0 || remaining < next)) {
            next = remaining;
        }
    }
    return next;
}
