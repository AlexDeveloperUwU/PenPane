#include "hotkeys.h"

#include <string.h>
#include <wchar.h>

#define HOTKEY_ID_BASE 0x4C00
#define TIMER_GESTURE HOTKEYS_TIMER_ID
#define TIMER_LEARN (HOTKEYS_TIMER_ID + 1)
#define TIMER_EVENTS (HOTKEYS_TIMER_ID + 2)
#define POLL_INTERVAL_MS 12
#define LEARN_TIMEOUT_MS 15000
#define DEFAULT_DOUBLE_WINDOW_MS 300
#define DEFAULT_LONG_PRESS_MS 600
#define ACTION_QUEUE_SIZE 32
#define EVENT_QUEUE_SIZE 32
#define MOUSE_BUTTON_COUNT 3
#define MODIFIER_KEY_COUNT 11
#define KEY_BITSET_WORDS 8
#define MESSAGE_TEXT_SIZE 256
#define REPORT_LINE_SIZE 320

/** @brief A mouse press or release captured by the low-level hook and handled outside it. */
typedef struct InputEvent {
    Trigger trigger;
    bool down;
    uint32_t time;
} InputEvent;

/** @brief Maps a modifier virtual key to its TRIGGER_MOD_* bit. */
typedef struct ModifierKey {
    unsigned virtualKey;
    unsigned bit;
} ModifierKey;

/** @brief One keyboard hotkey registered with the system. */
typedef struct RegisteredKey {
    int id;
    Trigger trigger;
} RegisteredKey;

/** @brief State of an ongoing learn-mode capture. */
typedef struct LearnState {
    bool active;
    bool decided;
    bool captured;
    bool haveTrigger;
    bool down;
    bool awaitingSecond;
    bool second;
    Action action;
    Trigger trigger;
    uint32_t pressedAt;
    unsigned modifiersDown;
    uint32_t swallowedKeys[KEY_BITSET_WORDS];
    bool swallowedMouse[MOUSE_BUTTON_COUNT];
    Binding result;
} LearnState;

/** @brief Hotkey module instance. */
struct Hotkeys {
    HWND window;
    HotkeyActionHandler onAction;
    HotkeyLearnHandler onLearn;
    void *user;
    GestureEngine engine;
    int doubleWindowMs;
    int longPressMs;
    RegisteredKey registered[GESTURE_MAX_TRACKED];
    int registeredCount;
    HHOOK mouseHook;
    HHOOK keyboardHook;
    bool mouseBindingsExist;
    bool swallowedMouse[MOUSE_BUTTON_COUNT];
    Trigger swallowedTrigger[MOUSE_BUTTON_COUNT];
    int timerInterval;
    Action actionQueue[ACTION_QUEUE_SIZE];
    int actionHead;
    int actionCount;
    InputEvent eventQueue[EVENT_QUEUE_SIZE];
    int eventHead;
    int eventCount;
    LearnState learn;
};

/** @brief The single instance that owns the process-wide low-level hooks. */
static Hotkeys *g_owner;

/** @brief Modifier virtual keys as reported by the low-level keyboard hook. */
static const ModifierKey MODIFIER_KEYS[MODIFIER_KEY_COUNT] = {
    {VK_SHIFT, TRIGGER_MOD_SHIFT},     {VK_LSHIFT, TRIGGER_MOD_SHIFT},
    {VK_RSHIFT, TRIGGER_MOD_SHIFT},    {VK_CONTROL, TRIGGER_MOD_CONTROL},
    {VK_LCONTROL, TRIGGER_MOD_CONTROL}, {VK_RCONTROL, TRIGGER_MOD_CONTROL},
    {VK_MENU, TRIGGER_MOD_ALT},        {VK_LMENU, TRIGGER_MOD_ALT},
    {VK_RMENU, TRIGGER_MOD_ALT},       {VK_LWIN, TRIGGER_MOD_WIN},
    {VK_RWIN, TRIGGER_MOD_WIN},
};

/**
 * @brief Appends text to a wide buffer, truncating cleanly when it is full.
 * @param buffer Destination buffer.
 * @param size Capacity of the buffer in characters.
 * @param text Zero-terminated text to append.
 */
static void wide_append(wchar_t *buffer, int size, const wchar_t *text)
{
    size_t used = wcslen(buffer);
    size_t room = (size_t)size - used - 1;
    size_t length = wcslen(text);
    if (length > room) {
        length = room;
    }
    memcpy(buffer + used, text, length * sizeof(wchar_t));
    buffer[used + length] = L'\0';
}

/**
 * @brief Appends one report line describing a registration failure.
 * @param report Report buffer, or NULL.
 * @param reportSize Capacity of the report buffer.
 * @param trigger Trigger that failed to register.
 * @param error Win32 error code from RegisterHotKey.
 */
static void report_failure(wchar_t *report, int reportSize, Trigger trigger, DWORD error)
{
    if (report == NULL || reportSize <= 0) {
        return;
    }
    wchar_t line[REPORT_LINE_SIZE];
    wchar_t message[MESSAGE_TEXT_SIZE];
    line[0] = L'\0';
    trigger_to_text(trigger, line, 64);
    wide_append(line, REPORT_LINE_SIZE, L": ");
    DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0,
                                  message, MESSAGE_TEXT_SIZE, NULL);
    while (length > 0 && (message[length - 1] == L'\r' || message[length - 1] == L'\n' ||
                          message[length - 1] == L' ')) {
        length--;
    }
    if (length == 0) {
        wide_append(line, REPORT_LINE_SIZE, L"registration failed, error ");
        wchar_t digits[12];
        int count = 0;
        DWORD value = error;
        do {
            digits[count++] = (wchar_t)(L'0' + value % 10u);
            value /= 10u;
        } while (value != 0 && count < 11);
        wchar_t number[12];
        for (int i = 0; i < count; i++) {
            number[i] = digits[count - 1 - i];
        }
        number[count] = L'\0';
        wide_append(line, REPORT_LINE_SIZE, number);
    } else {
        message[length] = L'\0';
        wide_append(line, REPORT_LINE_SIZE, message);
    }
    wide_append(line, REPORT_LINE_SIZE, L"\n");
    wide_append(report, reportSize, line);
}

/**
 * @brief Looks up the index of a modifier virtual key.
 * @param virtualKey Virtual-key code.
 * @return Index into MODIFIER_KEYS, or -1 when the key is not a modifier.
 */
static int modifier_index(unsigned virtualKey)
{
    for (int i = 0; i < MODIFIER_KEY_COUNT; i++) {
        if (MODIFIER_KEYS[i].virtualKey == virtualKey) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Reads the modifiers that are physically held right now.
 * @return TRIGGER_MOD_* bits.
 */
static unsigned current_modifiers(void)
{
    unsigned modifiers = 0;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) {
        modifiers |= TRIGGER_MOD_CONTROL;
    }
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
        modifiers |= TRIGGER_MOD_ALT;
    }
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
        modifiers |= TRIGGER_MOD_SHIFT;
    }
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
        modifiers |= TRIGGER_MOD_WIN;
    }
    return modifiers;
}

/**
 * @brief Converts the held-modifier bit mask of learn mode into TRIGGER_MOD_* bits.
 * @param learn Learn state.
 * @return TRIGGER_MOD_* bits of the modifiers currently held.
 */
static unsigned learn_modifiers(const LearnState *learn)
{
    unsigned modifiers = 0;
    for (int i = 0; i < MODIFIER_KEY_COUNT; i++) {
        if ((learn->modifiersDown & (1u << i)) != 0) {
            modifiers |= MODIFIER_KEYS[i].bit;
        }
    }
    return modifiers;
}

/**
 * @brief Queues an action produced by the gesture engine for delivery outside engine calls.
 * @param user Hotkeys instance.
 * @param action Resolved action.
 */
static void engine_sink(void *user, Action action)
{
    Hotkeys *hotkeys = user;
    if (hotkeys->actionCount >= ACTION_QUEUE_SIZE) {
        return;
    }
    hotkeys->actionQueue[(hotkeys->actionHead + hotkeys->actionCount) % ACTION_QUEUE_SIZE] = action;
    hotkeys->actionCount++;
}

/**
 * @brief Delivers queued actions to the owner's handler.
 * @param hotkeys Instance.
 * @return true when at least one action was delivered.
 */
static bool flush_actions(Hotkeys *hotkeys)
{
    bool delivered = false;
    while (hotkeys->actionCount > 0) {
        Action action = hotkeys->actionQueue[hotkeys->actionHead];
        hotkeys->actionHead = (hotkeys->actionHead + 1) % ACTION_QUEUE_SIZE;
        hotkeys->actionCount--;
        delivered = true;
        if (hotkeys->onAction != NULL) {
            hotkeys->onAction(hotkeys->user, action);
        }
    }
    return delivered;
}

/**
 * @brief Tells whether any keyboard trigger is currently held according to the engine.
 * @param hotkeys Instance.
 * @return true when the release poll must keep running.
 */
static bool any_key_held(const Hotkeys *hotkeys)
{
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        const GestureTracked *slot = &hotkeys->engine.tracked[i];
        if (slot->inUse && slot->down && slot->trigger.kind == TRIGGER_KEY) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Reports releases of held keyboard triggers that are no longer physically down.
 * @param hotkeys Instance.
 * @param nowMs Current clock value.
 */
static void poll_releases(Hotkeys *hotkeys, uint32_t nowMs)
{
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        const GestureTracked *slot = &hotkeys->engine.tracked[i];
        if (slot->inUse && slot->down && slot->trigger.kind == TRIGGER_KEY &&
            (GetAsyncKeyState((int)slot->trigger.virtualKey) & 0x8000) == 0) {
            gesture_release(&hotkeys->engine, slot->trigger, nowMs);
        }
    }
}

/**
 * @brief Starts, retargets or stops the single gesture timer.
 * @param hotkeys Instance.
 * @param interval Milliseconds until the next wake-up, or 0 or less to stop the timer.
 */
static void schedule_timer(Hotkeys *hotkeys, int interval)
{
    if (interval <= 0) {
        if (hotkeys->timerInterval != 0) {
            KillTimer(hotkeys->window, TIMER_GESTURE);
            hotkeys->timerInterval = 0;
        }
        return;
    }
    if (interval == POLL_INTERVAL_MS && hotkeys->timerInterval == POLL_INTERVAL_MS) {
        return;
    }
    if (SetTimer(hotkeys->window, TIMER_GESTURE, (UINT)interval, NULL) != 0) {
        hotkeys->timerInterval = interval;
    }
}

/**
 * @brief Polls releases, fires due gestures, delivers actions and re-arms the timer for the next need.
 * @param hotkeys Instance.
 */
static void pump(Hotkeys *hotkeys)
{
    uint32_t now = (uint32_t)GetTickCount();
    poll_releases(hotkeys, now);
    int next;
    do {
        next = gesture_tick(&hotkeys->engine, now);
    } while (flush_actions(hotkeys));
    if (any_key_held(hotkeys)) {
        schedule_timer(hotkeys, POLL_INTERVAL_MS);
    } else {
        schedule_timer(hotkeys, next);
    }
}

/**
 * @brief Asks the window to process queued hook events soon, outside the hook procedure.
 * @param hotkeys Instance.
 */
static void post_events(const Hotkeys *hotkeys)
{
    PostMessageW(hotkeys->window, WM_TIMER, TIMER_EVENTS, 0);
}

/**
 * @brief Queues a mouse event for later handling.
 * @param hotkeys Instance.
 * @param trigger Trigger of the event.
 * @param down true for a press, false for a release.
 * @param time Event time in milliseconds.
 */
static void queue_event(Hotkeys *hotkeys, Trigger trigger, bool down, uint32_t time)
{
    if (hotkeys->eventCount >= EVENT_QUEUE_SIZE) {
        return;
    }
    InputEvent *event = &hotkeys->eventQueue[(hotkeys->eventHead + hotkeys->eventCount) % EVENT_QUEUE_SIZE];
    event->trigger = trigger;
    event->down = down;
    event->time = time;
    hotkeys->eventCount++;
    post_events(hotkeys);
}

/**
 * @brief Records the final outcome of learn mode from inside a hook and wakes the window.
 * @param hotkeys Instance.
 * @param captured true when a gesture was captured.
 * @param gesture Captured gesture, used only when captured is true.
 */
static void learn_decide(Hotkeys *hotkeys, bool captured, Gesture gesture)
{
    LearnState *learn = &hotkeys->learn;
    learn->decided = true;
    learn->captured = captured;
    learn->result.trigger = learn->trigger;
    learn->result.gesture = gesture;
    learn->result.action = learn->action;
    post_events(hotkeys);
}

/**
 * @brief Feeds one captured press or release of a key or button into the learn classifier.
 * @param hotkeys Instance.
 * @param trigger Trigger including the modifiers held at the time.
 * @param down true for a press, false for a release.
 * @param time Event time in milliseconds.
 */
static void learn_input(Hotkeys *hotkeys, Trigger trigger, bool down, uint32_t time)
{
    LearnState *learn = &hotkeys->learn;
    if (learn->decided) {
        return;
    }
    if (!learn->haveTrigger) {
        if (down) {
            learn->trigger = trigger;
            learn->haveTrigger = true;
            learn->down = true;
            learn->pressedAt = time;
        }
        return;
    }
    if (trigger.kind != learn->trigger.kind || trigger.virtualKey != learn->trigger.virtualKey) {
        return;
    }
    if (down) {
        if (learn->down) {
            return;
        }
        learn->down = true;
        if (learn->awaitingSecond) {
            learn->awaitingSecond = false;
            learn->second = true;
            SetTimer(hotkeys->window, TIMER_LEARN, LEARN_TIMEOUT_MS, NULL);
        }
        return;
    }
    if (!learn->down) {
        return;
    }
    learn->down = false;
    if (learn->second) {
        learn_decide(hotkeys, true, GESTURE_DOUBLE);
    } else if ((int32_t)(time - learn->pressedAt) >= hotkeys->longPressMs) {
        learn_decide(hotkeys, true, GESTURE_LONG);
    } else {
        learn->awaitingSecond = true;
        SetTimer(hotkeys->window, TIMER_LEARN, (UINT)hotkeys->doubleWindowMs, NULL);
    }
}

/**
 * @brief Tests one bit of a 256-bit key set.
 * @param set Bit set of KEY_BITSET_WORDS words.
 * @param index Bit index below 256.
 * @return true when the bit is set.
 */
static bool bitset_test(const uint32_t *set, unsigned index)
{
    return (set[index >> 5] & (1u << (index & 31u))) != 0;
}

/**
 * @brief Sets or clears one bit of a 256-bit key set.
 * @param set Bit set of KEY_BITSET_WORDS words.
 * @param index Bit index below 256.
 * @param value New bit value.
 */
static void bitset_assign(uint32_t *set, unsigned index, bool value)
{
    if (value) {
        set[index >> 5] |= 1u << (index & 31u);
    } else {
        set[index >> 5] &= ~(1u << (index & 31u));
    }
}

/**
 * @brief Handles a low-level keyboard event while learn mode is active.
 * @param hotkeys Instance.
 * @param info Hook data of the event.
 * @param down true for a key press, false for a release.
 * @return true when the event must be swallowed.
 */
static bool learn_keyboard(Hotkeys *hotkeys, const KBDLLHOOKSTRUCT *info, bool down)
{
    LearnState *learn = &hotkeys->learn;
    unsigned vk = info->vkCode;
    if (vk >= 256u) {
        return false;
    }
    int modifier = modifier_index(vk);
    if (!down) {
        if (!bitset_test(learn->swallowedKeys, vk)) {
            return false;
        }
        bitset_assign(learn->swallowedKeys, vk, false);
        if (modifier >= 0) {
            learn->modifiersDown &= ~(1u << modifier);
        } else {
            Trigger trigger = {TRIGGER_KEY, vk, learn_modifiers(learn)};
            learn_input(hotkeys, trigger, false, info->time);
        }
        return true;
    }
    bitset_assign(learn->swallowedKeys, vk, true);
    if (modifier >= 0) {
        learn->modifiersDown |= 1u << modifier;
        return true;
    }
    unsigned modifiers = learn_modifiers(learn);
    bool emergency = modifiers == (TRIGGER_MOD_CONTROL | TRIGGER_MOD_ALT | TRIGGER_MOD_SHIFT);
    if (vk == VK_ESCAPE && (modifiers == 0 || emergency) && !learn->decided) {
        learn_decide(hotkeys, false, GESTURE_SINGLE);
        return true;
    }
    Trigger trigger = {TRIGGER_KEY, vk, modifiers};
    learn_input(hotkeys, trigger, true, info->time);
    return true;
}

/**
 * @brief Handles a mouse button event while learn mode is active.
 * @param hotkeys Instance.
 * @param button VK_MBUTTON, VK_XBUTTON1 or VK_XBUTTON2.
 * @param down true for a press, false for a release.
 * @param time Event time in milliseconds.
 * @return true when the event must be swallowed.
 */
static bool learn_mouse(Hotkeys *hotkeys, unsigned button, bool down, uint32_t time)
{
    LearnState *learn = &hotkeys->learn;
    unsigned index = button - VK_MBUTTON;
    Trigger trigger = {TRIGGER_MOUSE_BUTTON, button, down ? current_modifiers() : 0};
    if (!down) {
        if (!learn->swallowedMouse[index]) {
            return false;
        }
        learn->swallowedMouse[index] = false;
        learn_input(hotkeys, trigger, false, time);
        return true;
    }
    learn->swallowedMouse[index] = true;
    learn_input(hotkeys, trigger, true, time);
    return true;
}

/**
 * @brief Routes a mouse button event to learn mode or to the normal binding table.
 * @param hotkeys Instance.
 * @param button VK_MBUTTON, VK_XBUTTON1 or VK_XBUTTON2.
 * @param down true for a press, false for a release.
 * @param time Event time in milliseconds.
 * @return true when the event must be swallowed.
 */
static bool route_mouse_button(Hotkeys *hotkeys, unsigned button, bool down, uint32_t time)
{
    unsigned index = button - VK_MBUTTON;
    if (!down && hotkeys->swallowedMouse[index]) {
        hotkeys->swallowedMouse[index] = false;
        queue_event(hotkeys, hotkeys->swallowedTrigger[index], false, time);
        return true;
    }
    if (hotkeys->learn.active) {
        return learn_mouse(hotkeys, button, down, time);
    }
    if (!down) {
        return false;
    }
    Trigger trigger = {TRIGGER_MOUSE_BUTTON, button, current_modifiers()};
    if (!gesture_is_bound(&hotkeys->engine, trigger)) {
        return false;
    }
    hotkeys->swallowedMouse[index] = true;
    hotkeys->swallowedTrigger[index] = trigger;
    queue_event(hotkeys, trigger, true, time);
    return true;
}

/**
 * @brief Low-level mouse hook: classifies middle and X buttons and hands them off without doing real work.
 * @param code Hook code.
 * @param wparam Mouse message.
 * @param lparam Pointer to MSLLHOOKSTRUCT.
 * @return 1 to swallow the event, otherwise the result of the next hook.
 */
static LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    Hotkeys *hotkeys = g_owner;
    if (code == HC_ACTION && hotkeys != NULL) {
        const MSLLHOOKSTRUCT *info = (const MSLLHOOKSTRUCT *)lparam;
        unsigned button = 0;
        bool down = false;
        switch (wparam) {
        case WM_MBUTTONDOWN:
            button = VK_MBUTTON;
            down = true;
            break;
        case WM_MBUTTONUP:
            button = VK_MBUTTON;
            break;
        case WM_XBUTTONDOWN:
            down = true;
            button = HIWORD(info->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
            break;
        case WM_XBUTTONUP:
            button = HIWORD(info->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
            break;
        default:
            break;
        }
        if (button != 0 && route_mouse_button(hotkeys, button, down, (uint32_t)info->time)) {
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

/**
 * @brief Low-level keyboard hook, active only during learn mode.
 * @param code Hook code.
 * @param wparam Keyboard message.
 * @param lparam Pointer to KBDLLHOOKSTRUCT.
 * @return 1 to swallow the event, otherwise the result of the next hook.
 */
static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    Hotkeys *hotkeys = g_owner;
    if (code == HC_ACTION && hotkeys != NULL && hotkeys->learn.active) {
        bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
        bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
        if ((down || up) && learn_keyboard(hotkeys, (const KBDLLHOOKSTRUCT *)lparam, down)) {
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

/**
 * @brief Installs or removes the mouse hook so that it exists only while it has work to do.
 * @param hotkeys Instance.
 */
static void update_mouse_hook(Hotkeys *hotkeys)
{
    bool wanted = hotkeys->mouseBindingsExist || hotkeys->learn.active;
    if (wanted && hotkeys->mouseHook == NULL) {
        hotkeys->mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook_proc, GetModuleHandleW(NULL), 0);
    } else if (!wanted && hotkeys->mouseHook != NULL) {
        UnhookWindowsHookEx(hotkeys->mouseHook);
        hotkeys->mouseHook = NULL;
        memset(hotkeys->swallowedMouse, 0, sizeof hotkeys->swallowedMouse);
    }
}

/**
 * @brief Unregisters every keyboard hotkey of the instance.
 * @param hotkeys Instance.
 */
static void unregister_all(Hotkeys *hotkeys)
{
    for (int i = 0; i < hotkeys->registeredCount; i++) {
        UnregisterHotKey(hotkeys->window, hotkeys->registered[i].id);
    }
    hotkeys->registeredCount = 0;
}

/**
 * @brief Ends learn mode without calling the learn handler: removes hooks and timers.
 * @param hotkeys Instance.
 */
static void learn_stop(Hotkeys *hotkeys)
{
    if (!hotkeys->learn.active) {
        return;
    }
    hotkeys->learn.active = false;
    KillTimer(hotkeys->window, TIMER_LEARN);
    if (hotkeys->keyboardHook != NULL) {
        UnhookWindowsHookEx(hotkeys->keyboardHook);
        hotkeys->keyboardHook = NULL;
    }
    update_mouse_hook(hotkeys);
}

/**
 * @brief Ends learn mode and reports the outcome to the learn handler.
 * @param hotkeys Instance.
 * @param captured true when a gesture was captured.
 * @param binding Captured binding, meaningful when captured is true.
 */
static void learn_finish(Hotkeys *hotkeys, bool captured, Binding binding)
{
    Action action = hotkeys->learn.action;
    learn_stop(hotkeys);
    if (hotkeys->onLearn != NULL) {
        hotkeys->onLearn(hotkeys->user, action, binding, captured);
    }
}

/**
 * @brief Handles queued mouse events and a pending learn-mode decision.
 * @param hotkeys Instance.
 */
static void handle_events(Hotkeys *hotkeys)
{
    while (hotkeys->eventCount > 0) {
        InputEvent event = hotkeys->eventQueue[hotkeys->eventHead];
        hotkeys->eventHead = (hotkeys->eventHead + 1) % EVENT_QUEUE_SIZE;
        hotkeys->eventCount--;
        if (event.down) {
            gesture_press(&hotkeys->engine, event.trigger, event.time);
        } else {
            gesture_release(&hotkeys->engine, event.trigger, event.time);
        }
    }
    pump(hotkeys);
    if (hotkeys->learn.active && hotkeys->learn.decided) {
        Binding result = hotkeys->learn.result;
        learn_finish(hotkeys, hotkeys->learn.captured, result);
    }
}

/**
 * @brief Handles the learn-mode timer: end of the double window or overall timeout.
 * @param hotkeys Instance.
 */
static void handle_learn_timer(Hotkeys *hotkeys)
{
    LearnState *learn = &hotkeys->learn;
    if (!learn->active) {
        KillTimer(hotkeys->window, TIMER_LEARN);
        return;
    }
    if (learn->decided) {
        return;
    }
    if (learn->awaitingSecond && !learn->down) {
        Binding result = {learn->trigger, GESTURE_SINGLE, learn->action};
        learn_finish(hotkeys, true, result);
        return;
    }
    Binding none;
    memset(&none, 0, sizeof none);
    learn_finish(hotkeys, false, none);
}

Hotkeys *hotkeys_create(HWND window, HotkeyActionHandler onAction, HotkeyLearnHandler onLearn, void *user)
{
    if (window == NULL || g_owner != NULL) {
        return NULL;
    }
    Hotkeys *hotkeys = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *hotkeys);
    if (hotkeys == NULL) {
        return NULL;
    }
    hotkeys->window = window;
    hotkeys->onAction = onAction;
    hotkeys->onLearn = onLearn;
    hotkeys->user = user;
    hotkeys->doubleWindowMs = DEFAULT_DOUBLE_WINDOW_MS;
    hotkeys->longPressMs = DEFAULT_LONG_PRESS_MS;
    gesture_init(&hotkeys->engine, NULL, 0, hotkeys->doubleWindowMs, hotkeys->longPressMs, engine_sink,
                 hotkeys);
    g_owner = hotkeys;
    return hotkeys;
}

void hotkeys_destroy(Hotkeys *hotkeys)
{
    if (hotkeys == NULL) {
        return;
    }
    learn_stop(hotkeys);
    unregister_all(hotkeys);
    schedule_timer(hotkeys, 0);
    hotkeys->mouseBindingsExist = false;
    update_mouse_hook(hotkeys);
    if (g_owner == hotkeys) {
        g_owner = NULL;
    }
    HeapFree(GetProcessHeap(), 0, hotkeys);
}

bool hotkeys_apply(Hotkeys *hotkeys, const Config *config, wchar_t *report, int reportSize)
{
    if (report != NULL && reportSize > 0) {
        report[0] = L'\0';
    }
    if (hotkeys == NULL || config == NULL) {
        return false;
    }
    unregister_all(hotkeys);
    schedule_timer(hotkeys, 0);
    hotkeys->actionCount = 0;
    hotkeys->eventCount = 0;
    int count = config->bindingCount;
    if (count < 0) {
        count = 0;
    }
    hotkeys->doubleWindowMs = config->doubleWindowMs;
    hotkeys->longPressMs = config->longPressMs;
    gesture_init(&hotkeys->engine, config->bindings, count, config->doubleWindowMs, config->longPressMs,
                 engine_sink, hotkeys);
    bool allRegistered = true;
    hotkeys->mouseBindingsExist = false;
    for (int i = 0; i < GESTURE_MAX_TRACKED; i++) {
        const GestureTracked *slot = &hotkeys->engine.tracked[i];
        if (!slot->inUse) {
            continue;
        }
        if (slot->trigger.kind == TRIGGER_MOUSE_BUTTON) {
            hotkeys->mouseBindingsExist = true;
            continue;
        }
        int id = HOTKEY_ID_BASE + hotkeys->registeredCount;
        if (RegisterHotKey(hotkeys->window, id, slot->trigger.modifiers, slot->trigger.virtualKey)) {
            hotkeys->registered[hotkeys->registeredCount].id = id;
            hotkeys->registered[hotkeys->registeredCount].trigger = slot->trigger;
            hotkeys->registeredCount++;
        } else {
            allRegistered = false;
            report_failure(report, reportSize, slot->trigger, GetLastError());
        }
    }
    update_mouse_hook(hotkeys);
    if (hotkeys->mouseBindingsExist && hotkeys->mouseHook == NULL) {
        allRegistered = false;
        report_failure(report, reportSize, (Trigger){TRIGGER_MOUSE_BUTTON, VK_MBUTTON, 0}, GetLastError());
    }
    return allRegistered;
}

bool hotkeys_handle_message(Hotkeys *hotkeys, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (hotkeys == NULL) {
        return false;
    }
    if (message == WM_HOTKEY) {
        int index = (int)wparam - HOTKEY_ID_BASE;
        if (index < 0 || index >= hotkeys->registeredCount) {
            return false;
        }
        gesture_press(&hotkeys->engine, hotkeys->registered[index].trigger, (uint32_t)GetTickCount());
        pump(hotkeys);
        return true;
    }
    if (message != WM_TIMER) {
        return false;
    }
    if (wparam == TIMER_GESTURE) {
        pump(hotkeys);
        return true;
    }
    if (wparam == TIMER_LEARN) {
        handle_learn_timer(hotkeys);
        return true;
    }
    if (wparam == TIMER_EVENTS) {
        handle_events(hotkeys);
        return true;
    }
    return false;
}

bool hotkeys_learn_begin(Hotkeys *hotkeys, Action action)
{
    if (hotkeys == NULL || hotkeys->learn.active) {
        return false;
    }
    memset(&hotkeys->learn, 0, sizeof hotkeys->learn);
    hotkeys->learn.action = action;
    hotkeys->keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook_proc, GetModuleHandleW(NULL), 0);
    if (hotkeys->keyboardHook == NULL) {
        return false;
    }
    hotkeys->learn.active = true;
    update_mouse_hook(hotkeys);
    SetTimer(hotkeys->window, TIMER_LEARN, LEARN_TIMEOUT_MS, NULL);
    return true;
}

void hotkeys_learn_cancel(Hotkeys *hotkeys)
{
    if (hotkeys == NULL || !hotkeys->learn.active) {
        return;
    }
    Binding none;
    memset(&none, 0, sizeof none);
    learn_finish(hotkeys, false, none);
}
