#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/gesture.h"

#define LOG_CAPACITY 32

/** @brief Records every action delivered by the engine. */
typedef struct Recorder {
    Action log[LOG_CAPACITY];
    int count;
} Recorder;

/**
 * @brief Sink that appends the action to a Recorder.
 * @param user Recorder pointer.
 * @param action Delivered action.
 */
static void record(void *user, Action action)
{
    Recorder *recorder = user;
    assert(recorder->count < LOG_CAPACITY);
    recorder->log[recorder->count++] = action;
}

/**
 * @brief Builds a keyboard trigger without modifiers.
 * @param virtualKey Virtual-key code.
 * @return Trigger value.
 */
static Trigger key(unsigned virtualKey)
{
    Trigger trigger = {TRIGGER_KEY, virtualKey, 0};
    return trigger;
}

/**
 * @brief Builds a binding.
 * @param trigger Trigger of the binding.
 * @param gesture Gesture of the binding.
 * @param action Action of the binding.
 * @return Binding value.
 */
static Binding bind(Trigger trigger, Gesture gesture, Action action)
{
    Binding binding = {trigger, gesture, action};
    return binding;
}

/** @brief A single-only trigger fires on press without delay and ignores auto-repeat. */
static void test_single_only(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_SINGLE, ACTION_UNDO)};
    gesture_init(&engine, bindings, 1, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7C), 1000);
    assert(rec.count == 1 && rec.log[0] == ACTION_UNDO);
    assert(gesture_tick(&engine, 1000) == -1);

    gesture_press(&engine, key(0x7C), 1030);
    gesture_press(&engine, key(0x7C), 1060);
    assert(rec.count == 1);

    gesture_release(&engine, key(0x7C), 1100);
    assert(rec.count == 1);
    assert(gesture_tick(&engine, 5000) == -1);
    assert(rec.count == 1);

    gesture_press(&engine, key(0x7C), 5100);
    assert(rec.count == 2);
}

/** @brief A double binding delays the single until the window expires. */
static void test_double_suppresses_single(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_SINGLE, ACTION_UNDO),
                          bind(key(0x7C), GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7C), 0);
    assert(rec.count == 0);
    gesture_release(&engine, key(0x7C), 50);
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 50) == 300);
    assert(gesture_tick(&engine, 200) == 150);
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 349) == 1);
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 350) == -1);
    assert(rec.count == 1 && rec.log[0] == ACTION_UNDO);
}

/** @brief A second press inside the window fires only the double. */
static void test_double_fires(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_SINGLE, ACTION_UNDO),
                          bind(key(0x7C), GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7C), 0);
    gesture_release(&engine, key(0x7C), 50);
    gesture_press(&engine, key(0x7C), 200);
    assert(rec.count == 1 && rec.log[0] == ACTION_TOGGLE_OVERLAY);
    gesture_release(&engine, key(0x7C), 250);
    assert(gesture_tick(&engine, 250) == -1);
    assert(gesture_tick(&engine, 2000) == -1);
    assert(rec.count == 1);
}

/** @brief A late second press, with no tick in between, still resolves the first as a single. */
static void test_double_window_expired_without_tick(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_SINGLE, ACTION_UNDO),
                          bind(key(0x7C), GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7C), 0);
    gesture_release(&engine, key(0x7C), 50);
    gesture_press(&engine, key(0x7C), 400);
    assert(rec.count == 1 && rec.log[0] == ACTION_UNDO);
    gesture_release(&engine, key(0x7C), 450);
    assert(gesture_tick(&engine, 450) == 300);
    assert(gesture_tick(&engine, 750) == -1);
    assert(rec.count == 2 && rec.log[1] == ACTION_UNDO);
}

/** @brief A long binding fires while the key is still held and is not followed by a single. */
static void test_long_fires_while_held(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7D), GESTURE_SINGLE, ACTION_NEXT_TOOL),
                          bind(key(0x7D), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7D), 100);
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 100) == 600);
    assert(gesture_tick(&engine, 400) == 300);
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 700) == -1);
    assert(rec.count == 1 && rec.log[0] == ACTION_CLEAR);
    gesture_release(&engine, key(0x7D), 900);
    assert(rec.count == 1);
    assert(gesture_tick(&engine, 2000) == -1);
}

/** @brief A press shorter than the long threshold becomes a single, on release. */
static void test_short_press_with_long_binding(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7D), GESTURE_SINGLE, ACTION_NEXT_TOOL),
                          bind(key(0x7D), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7D), 0);
    gesture_release(&engine, key(0x7D), 599);
    assert(rec.count == 1 && rec.log[0] == ACTION_NEXT_TOOL);
    assert(gesture_tick(&engine, 599) == -1);
}

/** @brief A release that arrives after the long threshold without a tick still counts as long. */
static void test_late_release_is_long(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7D), GESTURE_SINGLE, ACTION_NEXT_TOOL),
                          bind(key(0x7D), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7D), 0);
    gesture_release(&engine, key(0x7D), 700);
    assert(rec.count == 1 && rec.log[0] == ACTION_CLEAR);
}

/** @brief Single, double and long on one trigger resolve independently. */
static void test_all_three_gestures(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7D), GESTURE_SINGLE, ACTION_NEXT_TOOL),
                          bind(key(0x7D), GESTURE_DOUBLE, ACTION_TOGGLE_MODE),
                          bind(key(0x7D), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 3, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7D), 0);
    assert(gesture_tick(&engine, 700) == -1);
    gesture_release(&engine, key(0x7D), 800);
    assert(rec.count == 1 && rec.log[0] == ACTION_CLEAR);
    assert(gesture_tick(&engine, 800) == -1);

    gesture_press(&engine, key(0x7D), 1000);
    gesture_release(&engine, key(0x7D), 1050);
    assert(gesture_tick(&engine, 1400) == -1);
    assert(rec.count == 2 && rec.log[1] == ACTION_NEXT_TOOL);

    gesture_press(&engine, key(0x7D), 2000);
    gesture_release(&engine, key(0x7D), 2050);
    gesture_press(&engine, key(0x7D), 2150);
    assert(rec.count == 3 && rec.log[2] == ACTION_TOGGLE_MODE);
    gesture_release(&engine, key(0x7D), 2200);
    assert(gesture_tick(&engine, 9000) == -1);
    assert(rec.count == 3);
}

/** @brief Auto-repeat presses neither re-fire nor reset the long timer. */
static void test_auto_repeat_ignored(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7D), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 1, 300, 600, record, &rec);

    gesture_press(&engine, key(0x7D), 0);
    for (uint32_t t = 100; t < 600; t += 30) {
        gesture_press(&engine, key(0x7D), t);
    }
    assert(rec.count == 0);
    assert(gesture_tick(&engine, 600) == -1);
    assert(rec.count == 1);
    gesture_press(&engine, key(0x7D), 640);
    assert(rec.count == 1);
}

/** @brief Idle engines and unbound triggers need no timer and produce nothing. */
static void test_idle_and_unbound(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    gesture_init(&engine, NULL, 0, 300, 600, record, &rec);
    assert(gesture_tick(&engine, 0) == -1);
    gesture_press(&engine, key(0x41), 0);
    gesture_release(&engine, key(0x41), 10);
    assert(rec.count == 0);
    assert(!gesture_is_bound(&engine, key(0x41)));
    assert(!gesture_needs_release(&engine, key(0x41)));
}

/** @brief Bound and release queries reflect the binding table. */
static void test_queries(void)
{
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x70), GESTURE_SINGLE, ACTION_UNDO),
                          bind(key(0x71), GESTURE_DOUBLE, ACTION_REDO),
                          bind(key(0x72), GESTURE_LONG, ACTION_CLEAR)};
    gesture_init(&engine, bindings, 3, 300, 600, NULL, NULL);
    assert(gesture_is_bound(&engine, key(0x70)));
    assert(!gesture_needs_release(&engine, key(0x70)));
    assert(gesture_needs_release(&engine, key(0x71)));
    assert(gesture_needs_release(&engine, key(0x72)));
    gesture_press(&engine, key(0x70), 0);
    gesture_press(&engine, key(0x71), 0);
    gesture_release(&engine, key(0x71), 10);
    assert(gesture_tick(&engine, 10) == 300);
}

/** @brief Kind and modifiers distinguish triggers; later bindings override earlier ones. */
static void test_trigger_identity_and_override(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Trigger plain = key(0x7C);
    Trigger ctrl = {TRIGGER_KEY, 0x7C, TRIGGER_MOD_CONTROL};
    Trigger mouse = {TRIGGER_MOUSE_BUTTON, 0x05, 0};
    Trigger keyFive = {TRIGGER_KEY, 0x05, 0};
    Binding bindings[] = {bind(plain, GESTURE_SINGLE, ACTION_UNDO), bind(ctrl, GESTURE_SINGLE, ACTION_REDO),
                          bind(mouse, GESTURE_SINGLE, ACTION_CLEAR),
                          bind(plain, GESTURE_SINGLE, ACTION_COPY_PNG)};
    gesture_init(&engine, bindings, 4, 300, 600, record, &rec);
    assert(trigger_equals(plain, plain));
    assert(!trigger_equals(plain, ctrl));
    assert(!trigger_equals(mouse, keyFive));
    assert(!gesture_is_bound(&engine, keyFive));
    gesture_press(&engine, plain, 0);
    gesture_release(&engine, plain, 10);
    gesture_press(&engine, ctrl, 20);
    gesture_release(&engine, ctrl, 30);
    gesture_press(&engine, mouse, 40);
    assert(rec.count == 3);
    assert(rec.log[0] == ACTION_COPY_PNG && rec.log[1] == ACTION_REDO && rec.log[2] == ACTION_CLEAR);
}

/** @brief Timing survives the 32-bit clock wrapping around. */
static void test_clock_wraparound(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_SINGLE, ACTION_UNDO),
                          bind(key(0x7C), GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY)};
    gesture_init(&engine, bindings, 2, 300, 600, record, &rec);
    uint32_t start = UINT32_MAX - 100u;
    gesture_press(&engine, key(0x7C), start);
    gesture_release(&engine, key(0x7C), start + 20u);
    assert(gesture_tick(&engine, start + 20u) == 300);
    assert(gesture_tick(&engine, start + 320u) == -1);
    assert(rec.count == 1 && rec.log[0] == ACTION_UNDO);
}

/** @brief A double-only trigger fires nothing for a single tap and then goes idle. */
static void test_double_only_single_tap(void)
{
    Recorder rec = {0};
    GestureEngine engine;
    Binding bindings[] = {bind(key(0x7C), GESTURE_DOUBLE, ACTION_TOGGLE_OVERLAY)};
    gesture_init(&engine, bindings, 1, 300, 600, record, &rec);
    gesture_press(&engine, key(0x7C), 0);
    gesture_release(&engine, key(0x7C), 40);
    assert(gesture_tick(&engine, 40) == 300);
    assert(gesture_tick(&engine, 400) == -1);
    assert(rec.count == 0);
}

/**
 * @brief Runs every gesture test.
 * @return 0 when all assertions hold.
 */
int main(void)
{
    test_single_only();
    test_double_suppresses_single();
    test_double_fires();
    test_double_window_expired_without_tick();
    test_long_fires_while_held();
    test_short_press_with_long_binding();
    test_late_release_is_long();
    test_all_three_gestures();
    test_auto_repeat_ignored();
    test_idle_and_unbound();
    test_queries();
    test_trigger_identity_and_override();
    test_clock_wraparound();
    test_double_only_single_tap();
    printf("test_gesture: 14 tests passed\n");
    return 0;
}
