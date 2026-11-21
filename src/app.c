#include "app.h"

#include <stdio.h>
#include <string.h>
#include <wtsapi32.h>

#include "clipboard.h"
#include "config.h"
#include "controller.h"
#include "hotkeys.h"
#include "mask_scratch.h"
#include "overlay.h"
#include "render.h"
#include "text.h"
#include "tray.h"
#include "ui.h"

#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_ACTIVATE (WM_APP + 2)
#define TIMER_ANIMATE 1
#define TIMER_FADE_WAIT 2
#define TIMER_FADE_STEP 3
#define ANIMATION_INTERVAL_MS 16
#define FADE_DURATION_MS 400
#define DISPLAY_LIMIT TRAY_MAX_MONITORS
#define HIT_TEST_BACKDROP 0x01000000u
#define MESSAGE_CLASS L"InkMessageWindow"
#define SINGLE_INSTANCE_MUTEX L"Local\\InkOverlaySingleInstance"
#define SCRATCH_KEEP_BYTES (256 * 1024)
#define PENDING_NOTICE_LIMIT 2

/** @brief A display and its areas in virtual-desktop coordinates. */
typedef struct Monitor {
    RECT bounds;
    RECT work;
} Monitor;

/** @brief A notification queued until the tray icon exists. */
typedef struct Notice {
    wchar_t title[64];
    wchar_t text[512];
} Notice;

/** @brief Whole-application state. */
typedef struct App {
    HINSTANCE instance;
    HWND messageWindow;
    HWND previousForeground;
    Config config;
    wchar_t configPath[MAX_PATH];
    Scene scenes[2];
    bool whiteboard;
    bool drawing;
    bool overlayReady;
    bool learning;
    bool quitting;
    Overlay overlay;
    Controller controller;
    Ui *ui;
    Tray *tray;
    Hotkeys *hotkeys;
    Monitor monitors[DISPLAY_LIMIT];
    int monitorCount;
    Tool previousTool;
    uint32_t fadeStartMs;
    bool pointerDown;
    UINT taskbarCreatedMessage;
    Notice pending[PENDING_NOTICE_LIMIT];
    int pendingCount;
} App;

static App app;

static void app_apply_state(void);
static void app_flush(void);
static void app_sync_ui(void);

/**
 * @brief Returns the scene of the current mode.
 * @return Pointer to the active scene.
 */
static Scene *active_scene(void)
{
    return &app.scenes[app.whiteboard ? 1 : 0];
}

/**
 * @brief Backdrop color for the current mode and state.
 * @return Transparent, hit-testable transparent, or opaque whiteboard color.
 */
static Color backdrop_color(void)
{
    if (app.whiteboard) {
        return COLOR_CANVAS_DARK;
    }
    return app.drawing ? HIT_TEST_BACKDROP : 0;
}

/**
 * @brief Collects the connected monitors, primary first.
 */
static BOOL CALLBACK collect_monitor(HMONITOR monitor, HDC context, LPRECT rect, LPARAM data)
{
    (void)context;
    (void)rect;
    MONITORINFO info = {.cbSize = sizeof info};
    if (app.monitorCount >= DISPLAY_LIMIT || !GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }
    Monitor entry = {info.rcMonitor, info.rcWork};
    if (info.dwFlags & MONITORINFOF_PRIMARY) {
        memmove(app.monitors + 1, app.monitors, sizeof(Monitor) * (size_t)app.monitorCount);
        app.monitors[0] = entry;
    } else {
        app.monitors[app.monitorCount] = entry;
    }
    app.monitorCount++;
    (void)data;
    return TRUE;
}

/**
 * @brief Re-reads the monitor layout and clamps configured monitor indexes.
 */
static void refresh_monitors(void)
{
    app.monitorCount = 0;
    EnumDisplayMonitors(NULL, NULL, collect_monitor, 0);
    if (app.monitorCount == 0) {
        RECT screen = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        app.monitors[0].bounds = app.monitors[0].work = screen;
        app.monitorCount = 1;
    }
    if (app.config.drawMonitor >= app.monitorCount || app.config.drawMonitor < 0) {
        app.config.drawMonitor = 0;
    }
    if (app.config.uiMonitor >= app.monitorCount || app.config.uiMonitor < 0) {
        app.config.uiMonitor = 0;
    }
}

/**
 * @brief Monotonic millisecond clock.
 * @return Milliseconds since system start.
 */
static uint32_t now_ms(void)
{
    return (uint32_t)GetTickCount64();
}

/**
 * @brief Gives the overlay keyboard focus, remembering the previously focused window.
 */
static void focus_overlay(void)
{
    HWND foreground = GetForegroundWindow();
    if (foreground == app.overlay.window) {
        return;
    }
    app.previousForeground = foreground;
    DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
    DWORD thisThread = GetCurrentThreadId();
    bool attached = foregroundThread && foregroundThread != thisThread &&
                    AttachThreadInput(thisThread, foregroundThread, TRUE);
    SetForegroundWindow(app.overlay.window);
    if (attached) {
        AttachThreadInput(thisThread, foregroundThread, FALSE);
    }
}

/**
 * @brief Returns focus to the window that had it before drawing started.
 */
static void restore_focus(void)
{
    HWND target = app.previousForeground;
    app.previousForeground = NULL;
    if (target && IsWindow(target) && IsWindowVisible(target) &&
        GetForegroundWindow() == app.overlay.window) {
        SetForegroundWindow(target);
    }
}

/**
 * @brief Repaints a region of the overlay surface from the scene and overlays.
 * @param region Pixel rectangle to repaint.
 */
static void render_region(const RECT *region)
{
    render_scene(&app.overlay.canvas, active_scene(), region, backdrop_color());
    controller_draw_overlays(&app.controller, &app.overlay.canvas, region, now_ms());
}

/**
 * @brief Repaints and presents the whole surface.
 */
static void render_everything(void)
{
    RECT all = canvas_area(&app.overlay.canvas);
    render_region(&all);
    overlay_present(&app.overlay, NULL);
}

/**
 * @brief Destroys the overlay window and surface, freeing their memory.
 */
static void destroy_overlay(void)
{
    if (!app.overlayReady) {
        return;
    }
    controller_cancel(&app.controller);
    controller_attach(&app.controller, active_scene(), NULL);
    overlay_destroy(&app.overlay);
    mask_scratch_release();
    app.overlayReady = false;
}

/**
 * @brief Creates the overlay window on the configured monitor.
 * @return true on success.
 */
static bool create_overlay(void);

/**
 * @brief Overlay window procedure; routes pointer, keyboard and cursor messages.
 * @param window Overlay window handle.
 * @param message Window message.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return Message result.
 */
static LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

static bool create_overlay(void)
{
    const Monitor *monitor = &app.monitors[app.config.drawMonitor];
    if (!overlay_create(&app.overlay, app.instance, monitor->bounds, overlay_procedure, NULL)) {
        overlay_destroy(&app.overlay);
        return false;
    }
    app.overlayReady = true;
    return true;
}

/**
 * @brief Computes which property sections apply to the current tool and selection.
 * @return Bitmask of UI_SECTION_* values.
 */
static unsigned sections_for_state(void)
{
    ElementType type = ELEMENT_PEN;
    const Element *selection = controller_selection(&app.controller);
    switch (app.controller.tool) {
    case TOOL_SELECT:
        if (!selection) {
            return 0;
        }
        type = selection->type;
        break;
    case TOOL_RECTANGLE:
        type = ELEMENT_RECTANGLE;
        break;
    case TOOL_ELLIPSE:
        type = ELEMENT_ELLIPSE;
        break;
    case TOOL_ARROW:
        type = ELEMENT_ARROW;
        break;
    case TOOL_LINE:
        type = ELEMENT_LINE;
        break;
    case TOOL_PEN:
        type = ELEMENT_PEN;
        break;
    case TOOL_TEXT:
        type = ELEMENT_TEXT;
        break;
    default:
        return 0;
    }
    switch (type) {
    case ELEMENT_RECTANGLE:
    case ELEMENT_ELLIPSE:
        return UI_SECTION_STROKE | UI_SECTION_FILL | UI_SECTION_WIDTH | UI_SECTION_STROKE_STYLE |
               UI_SECTION_OPACITY;
    case ELEMENT_ARROW:
    case ELEMENT_LINE:
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_STROKE_STYLE | UI_SECTION_OPACITY;
    case ELEMENT_PEN:
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_PRESSURE | UI_SECTION_OPACITY;
    case ELEMENT_TEXT:
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_OPACITY;
    }
    return 0;
}

static void app_sync_ui(void)
{
    if (!app.ui) {
        return;
    }
    const Element *selection = controller_selection(&app.controller);
    UiModel model;
    model.tool = app.controller.tool;
    model.toolLocked = app.controller.toolLocked;
    model.style = selection ? selection->style : app.controller.style;
    model.sections = sections_for_state();
    model.canUndo = scene_can_undo(active_scene());
    model.canRedo = scene_can_redo(active_scene());
    model.whiteboard = app.whiteboard;
    model.drawing = app.drawing;
    ui_update(app.ui, &model);
}

/**
 * @brief Starts or stops the trail animation timer according to controller state.
 */
/**
 * @brief Surface rectangle the overlay needs: the whole monitor while drawing, otherwise the annotation extent.
 * @return Rectangle in monitor coordinates; a single pixel when nothing is visible.
 */
static RECT wanted_surface(void)
{
    const Monitor *monitor = &app.monitors[app.config.drawMonitor];
    RECT full = {0, 0, monitor->bounds.right - monitor->bounds.left,
                 monitor->bounds.bottom - monitor->bounds.top};
    if (app.drawing || app.whiteboard) {
        return full;
    }
    const Scene *scene = active_scene();
    Dirty extent;
    dirty_reset(&extent);
    for (int i = 0; i < scene->count; i++) {
        dirty_add(&extent, element_bounds(&scene->items[i]));
    }
    RECT area;
    if (extent.empty || !rect_intersect(extent.rect, full, &area)) {
        RECT single = {0, 0, 1, 1};
        return single;
    }
    return area;
}

/**
 * @brief Gives the process working set back to the system; only for idle moments, never on a hot path.
 */
static void trim_working_set(void)
{
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static void update_animation_timer(void)
{
    if (app.controller.laser.count > 0 || app.controller.eraser.count > 0) {
        SetTimer(app.messageWindow, TIMER_ANIMATE, ANIMATION_INTERVAL_MS, NULL);
    }
}

static void app_flush(void)
{
    Dirty repaint;
    Dirty direct;
    controller_take_dirty(&app.controller, &repaint, &direct);
    if (!app.overlayReady) {
        return;
    }
    if (!app.drawing && !app.whiteboard && !repaint.empty) {
        RECT wanted = wanted_surface();
        RECT current = canvas_area(&app.overlay.canvas);
        if (!EqualRect(&wanted, &current) && overlay_set_surface(&app.overlay, wanted)) {
            render_everything();
            update_animation_timer();
            return;
        }
    }
    RECT surface = canvas_area(&app.overlay.canvas);
    Dirty total;
    dirty_reset(&total);
    RECT region;
    if (!repaint.empty && rect_intersect(repaint.rect, surface, &region)) {
        render_region(&region);
        dirty_add(&total, region);
    }
    if (!direct.empty && rect_intersect(direct.rect, surface, &region)) {
        dirty_add(&total, region);
    }
    if (!total.empty) {
        overlay_present(&app.overlay, &total.rect);
    }
    mask_scratch_trim(SCRATCH_KEEP_BYTES);
    update_animation_timer();
}

/**
 * @brief Cancels any pending annotation fade and restores full opacity.
 */
static void cancel_fade(void)
{
    KillTimer(app.messageWindow, TIMER_FADE_WAIT);
    KillTimer(app.messageWindow, TIMER_FADE_STEP);
    if (app.overlayReady) {
        overlay_set_opacity(&app.overlay, 255);
    }
}

static void app_apply_state(void)
{
    Scene *scene = active_scene();
    bool wantVisible = app.drawing || (!app.whiteboard && scene->count > 0);
    cancel_fade();
    if (!wantVisible) {
        if (app.overlayReady) {
            restore_focus();
        }
        destroy_overlay();
    } else {
        if (!app.overlayReady && !create_overlay()) {
            return;
        }
        overlay_set_bounds(&app.overlay, app.monitors[app.config.drawMonitor].bounds);
        if (!overlay_set_surface(&app.overlay, wanted_surface())) {
            destroy_overlay();
            return;
        }
        controller_attach(&app.controller, scene, &app.overlay.canvas);
        overlay_set_capture(&app.overlay, app.drawing);
        overlay_set_focusable(&app.overlay, app.drawing);
        render_everything();
        overlay_set_visible(&app.overlay, true);
        if (app.drawing) {
            focus_overlay();
        } else {
            restore_focus();
            SetWindowPos(app.overlay.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            if (app.config.autoFadeSeconds > 0 && scene->count > 0) {
                SetTimer(app.messageWindow, TIMER_FADE_WAIT, (UINT)app.config.autoFadeSeconds * 1000, NULL);
            }
        }
    }
    if (app.ui) {
        ui_set_visible(app.ui, true, app.drawing);
        ui_raise(app.ui);
    }
    app_sync_ui();
    if (!app.drawing) {
        trim_working_set();
    }
}

/**
 * @brief Runs the fade-out animation step by step and clears the annotations at the end.
 */
static void fade_step(void)
{
    uint32_t elapsed = now_ms() - app.fadeStartMs;
    if (!app.overlayReady || app.drawing) {
        cancel_fade();
        return;
    }
    if (elapsed >= FADE_DURATION_MS) {
        KillTimer(app.messageWindow, TIMER_FADE_STEP);
        scene_clear(&app.scenes[0]);
        app_apply_state();
        return;
    }
    overlay_set_opacity(&app.overlay, (unsigned char)(255 - 255 * elapsed / FADE_DURATION_MS));
    overlay_present(&app.overlay, NULL);
}

/**
 * @brief Enters or leaves drawing mode.
 * @param drawing true to capture input.
 */
static void set_drawing(bool drawing)
{
    if (!drawing) {
        controller_cancel(&app.controller);
        app_flush();
    }
    app.drawing = drawing;
    app_apply_state();
}

/**
 * @brief Switches between desktop and whiteboard modes.
 * @param whiteboard true for the opaque whiteboard.
 */
static void set_whiteboard(bool whiteboard)
{
    controller_cancel(&app.controller);
    app_flush();
    app.whiteboard = whiteboard;
    app.drawing = true;
    app.controller.selected = -1;
    app_apply_state();
}

/**
 * @brief Selects a tool through any path and refreshes the interface.
 * @param tool Tool to activate.
 */
static void select_tool(Tool tool)
{
    if (tool != app.controller.tool && tool != TOOL_LASER) {
        app.previousTool = app.controller.tool;
    }
    controller_set_tool(&app.controller, tool);
    app_flush();
    app_sync_ui();
}

/**
 * @brief Copies the active drawing to the clipboard as PNG.
 */
static void copy_drawing_png(void)
{
    Scene *scene = active_scene();
    RECT region = {0, 0, 0, 0};
    RECT surface = {0, 0, 0, 0};
    const Monitor *monitor = &app.monitors[app.config.drawMonitor];
    int width = monitor->bounds.right - monitor->bounds.left;
    int height = monitor->bounds.bottom - monitor->bounds.top;
    surface.right = width;
    surface.bottom = height;
    if (app.whiteboard) {
        region = surface;
    } else {
        Dirty extent;
        dirty_reset(&extent);
        for (int i = 0; i < scene->count; i++) {
            dirty_add(&extent, element_bounds(&scene->items[i]));
        }
        if (extent.empty) {
            return;
        }
        region = extent.rect;
    }
    Canvas export;
    if (!canvas_create(&export, width, height)) {
        return;
    }
    render_scene(&export, scene, &surface, app.whiteboard ? COLOR_CANVAS_DARK : 0);
    clipboard_copy_region(app.messageWindow, &export, region, app.whiteboard);
    canvas_destroy(&export);
}

/**
 * @brief Moves the selection by cycling through an ordered list.
 * @param step +1 for next, -1 for previous.
 */
static void cycle_tool(int step)
{
    int next = ((int)app.controller.tool + step + TOOL_COUNT) % TOOL_COUNT;
    select_tool((Tool)next);
}

/**
 * @brief Applies a style change to the defaults and the selected element.
 * @param style New style.
 */
static void change_style(Style style)
{
    app.controller.style = style;
    if (controller_selection(&app.controller)) {
        controller_apply_style_to_selection(&app.controller, &style);
    }
    app_flush();
    app_sync_ui();
}

/**
 * @brief Style currently shown and edited: the selection's or the defaults.
 * @return Style value.
 */
static Style current_style(void)
{
    const Element *selection = controller_selection(&app.controller);
    return selection ? selection->style : app.controller.style;
}

/**
 * @brief Performs a bound or UI-requested action.
 * @param action Action to perform.
 */
static void perform_action(Action action)
{
    Scene *scene = active_scene();
    Style style = current_style();
    switch (action) {
    case ACTION_TOGGLE_OVERLAY:
        set_drawing(!app.drawing);
        break;
    case ACTION_TOGGLE_MODE:
        set_whiteboard(!app.whiteboard);
        break;
    case ACTION_TOOL_SELECT:
    case ACTION_TOOL_RECTANGLE:
    case ACTION_TOOL_ELLIPSE:
    case ACTION_TOOL_ARROW:
    case ACTION_TOOL_LINE:
    case ACTION_TOOL_PEN:
    case ACTION_TOOL_LASER:
    case ACTION_TOOL_TEXT:
    case ACTION_TOOL_ERASER: {
        static const Tool MAP[] = {TOOL_SELECT, TOOL_RECTANGLE, TOOL_ELLIPSE, TOOL_ARROW, TOOL_LINE,
                                   TOOL_PEN,    TOOL_LASER,     TOOL_TEXT,    TOOL_ERASER};
        select_tool(MAP[action - ACTION_TOOL_SELECT]);
        break;
    }
    case ACTION_NEXT_TOOL:
        cycle_tool(1);
        break;
    case ACTION_PREVIOUS_TOOL:
        cycle_tool(-1);
        break;
    case ACTION_CYCLE_COLOR: {
        int index = style_stroke_index(style.stroke);
        style.stroke = STROKE_PALETTE[(index + 1) % STROKE_PALETTE_SIZE];
        change_style(style);
        break;
    }
    case ACTION_CYCLE_WIDTH: {
        int index = style_width_index(style.width);
        style.width = WIDTH_PRESETS[(index + 1) % WIDTH_PRESET_COUNT];
        change_style(style);
        break;
    }
    case ACTION_UNDO:
        controller_cancel(&app.controller);
        controller_deselect(&app.controller);
        scene_undo(scene);
        app_flush();
        app_sync_ui();
        break;
    case ACTION_REDO:
        controller_cancel(&app.controller);
        controller_deselect(&app.controller);
        scene_redo(scene);
        app_flush();
        app_sync_ui();
        break;
    case ACTION_CLEAR:
        controller_cancel(&app.controller);
        controller_deselect(&app.controller);
        scene_clear(scene);
        app_flush();
        app_sync_ui();
        break;
    case ACTION_TOGGLE_LASER:
        if (!app.drawing) {
            app.drawing = true;
            app_apply_state();
        }
        select_tool(app.controller.tool == TOOL_LASER ? app.previousTool : TOOL_LASER);
        break;
    case ACTION_TOGGLE_TOOL_LOCK:
        app.controller.toolLocked = !app.controller.toolLocked;
        app_sync_ui();
        break;
    case ACTION_COPY_PNG:
        copy_drawing_png();
        break;
    case ACTION_EMERGENCY_EXIT:
        app.quitting = true;
        PostQuitMessage(0);
        break;
    case ACTION_COUNT:
        break;
    }
}

/**
 * @brief Receives actions resolved from hotkeys and mouse buttons.
 * @param user Unused.
 * @param action Resolved action.
 */
static void on_hotkey_action(void *user, Action action)
{
    (void)user;
    perform_action(action);
}

/**
 * @brief Converts a trigger and gesture to readable text for notifications.
 * @param binding Binding to describe.
 * @param buffer Output buffer.
 * @param size Buffer size in characters.
 */
static void describe_binding(Binding binding, wchar_t *buffer, int size)
{
    static const wchar_t *GESTURES[] = {L"single press", L"double press", L"long press"};
    wchar_t trigger[64];
    trigger_to_text(binding.trigger, trigger, 64);
    _snwprintf(buffer, (size_t)size, L"%ls (%ls)", trigger, GESTURES[binding.gesture]);
    buffer[size - 1] = 0;
}

/**
 * @brief Shows a tray notification, or queues it while the tray icon is not created yet.
 * @param title Notification title.
 * @param text Notification body.
 */
static void notify(const wchar_t *title, const wchar_t *text)
{
    if (app.tray) {
        tray_notify(app.tray, title, text);
        return;
    }
    if (app.pendingCount < PENDING_NOTICE_LIMIT) {
        Notice *notice = &app.pending[app.pendingCount++];
        wcsncpy(notice->title, title, ARRAYSIZE(notice->title) - 1);
        wcsncpy(notice->text, text, ARRAYSIZE(notice->text) - 1);
    }
}

/**
 * @brief Creates the tray icon and delivers the notifications queued before it existed.
 */
static void create_tray(void)
{
    app.tray = tray_create(app.messageWindow, WM_APP_TRAY);
    for (int i = 0; i < app.pendingCount; i++) {
        notify(app.pending[i].title, app.pending[i].text);
    }
    app.pendingCount = 0;
}

/**
 * @brief Re-registers all hotkeys and reports failures through the tray.
 */
static void apply_hotkeys(void)
{
    wchar_t report[512] = L"";
    if (!hotkeys_apply(app.hotkeys, &app.config, report, 512) && report[0]) {
        notify(L"Some buttons could not be registered", report);
    }
}

/**
 * @brief Receives the result of learn mode and stores the new binding.
 * @param user Unused.
 * @param action Action being learned.
 * @param binding Captured binding.
 * @param captured false when learn mode ended without a capture.
 */
static void on_learn_result(void *user, Action action, Binding binding, bool captured)
{
    (void)user;
    app.learning = false;
    if (!captured) {
        notify(L"Learning cancelled", L"No button was bound.");
        return;
    }
    binding.action = action;
    config_set_binding(&app.config, binding);
    config_save(&app.config, app.configPath);
    apply_hotkeys();
    wchar_t text[160];
    describe_binding(binding, text, 160);
    notify(L"Button bound", text);
}

/**
 * @brief Applies a choice made in the floating UI.
 * @param user Unused.
 * @param event The choice.
 */
static void on_ui_event(void *user, const UiEvent *event)
{
    (void)user;
    Style style = current_style();
    switch (event->kind) {
    case UI_EVENT_ACTION:
        perform_action(event->action);
        break;
    case UI_EVENT_SELECT_TOOL:
        select_tool(event->tool);
        break;
    case UI_EVENT_SET_STROKE:
        style.stroke = STROKE_PALETTE[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_FILL:
        style.fill = FILL_PALETTE[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_WIDTH:
        style.width = WIDTH_PRESETS[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_STROKE_STYLE:
        style.strokeStyle = (StrokeStyle)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_PRESSURE:
        style.pressure = event->index != 0;
        change_style(style);
        break;
    case UI_EVENT_SET_OPACITY:
        style.opacity = event->value;
        change_style(style);
        break;
    case UI_EVENT_PANEL_MOVED:
        app.config.ui.placement[event->panel].set = true;
        app.config.ui.placement[event->panel].x = event->x;
        app.config.ui.placement[event->panel].y = event->y;
        config_save(&app.config, app.configPath);
        break;
    }
}

/**
 * @brief Builds a pointer sample from screen coordinates.
 * @param screen Pointer position in screen coordinates.
 * @param pressure Pressure 0..1 or negative when unknown.
 * @return Sample relative to the overlay surface.
 */
static PointerSample make_sample(POINT screen, float pressure)
{
    PointerSample sample;
    sample.position.x = (float)(screen.x - app.overlay.bounds.left);
    sample.position.y = (float)(screen.y - app.overlay.bounds.top);
    sample.pressure = pressure;
    sample.timeMs = now_ms();
    sample.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    return sample;
}

/**
 * @brief Finishes post-event work shared by every pointer path.
 */
static void after_pointer_event(void)
{
    if (controller_take_tool_finished(&app.controller) && !app.controller.toolLocked &&
        app.controller.tool != TOOL_ERASER && app.controller.tool != TOOL_LASER) {
        controller_set_tool(&app.controller, TOOL_SELECT);
    }
    app_flush();
    app_sync_ui();
}

/**
 * @brief Handles a press, move or release from either pointer messages or the mouse.
 * @param phase 0 down, 1 move, 2 up.
 * @param screen Screen coordinates.
 * @param pressure Pressure or negative when unknown.
 * @param contact true while the pointer touches or a button is held.
 */
static void handle_pointer(int phase, POINT screen, float pressure, bool contact)
{
    PointerSample sample = make_sample(screen, pressure);
    if (phase == 0) {
        app.pointerDown = true;
        controller_pointer_down(&app.controller, &sample);
    } else if (phase == 1) {
        controller_pointer_move(&app.controller, &sample, contact && app.pointerDown);
    } else {
        if (app.pointerDown) {
            controller_pointer_up(&app.controller, &sample);
        }
        app.pointerDown = false;
    }
    after_pointer_event();
}

/**
 * @brief Handles keyboard shortcuts that reach the focused overlay.
 * @param key Virtual key code.
 */
static void handle_key(WPARAM key)
{
    bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (app.controller.editingText) {
        if (key == VK_ESCAPE) {
            controller_text_commit(&app.controller);
            after_pointer_event();
        }
        return;
    }
    if (control && key == 'Z') {
        perform_action(shift ? ACTION_REDO : ACTION_UNDO);
    } else if (control && key == 'Y') {
        perform_action(ACTION_REDO);
    } else if (key == VK_DELETE || key == VK_BACK) {
        controller_delete_selection(&app.controller);
        app_flush();
        app_sync_ui();
    } else if (key == VK_ESCAPE) {
        select_tool(TOOL_SELECT);
    } else if (key == 'Q') {
        perform_action(ACTION_TOGGLE_TOOL_LOCK);
    } else if (!control) {
        Tool tool;
        if (tool_from_shortcut((int)key, &tool)) {
            select_tool(tool);
        }
    }
}

static LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        UINT32 id = GET_POINTERID_WPARAM(wparam);
        POINTER_INPUT_TYPE type = PT_POINTER;
        POINTER_INFO info;
        if (!GetPointerType(id, &type) || !GetPointerInfo(id, &info)) {
            break;
        }
        float pressure = -1.0f;
        if (type == PT_PEN) {
            POINTER_PEN_INFO pen;
            if (GetPointerPenInfo(id, &pen) && (pen.penMask & PEN_MASK_PRESSURE)) {
                pressure = (float)pen.pressure / 1024.0f;
            }
        }
        bool contact = IS_POINTER_INCONTACT_WPARAM(wparam);
        int phase = message == WM_POINTERDOWN ? 0 : (message == WM_POINTERUP ? 2 : 1);
        if (phase == 1 && !contact && app.pointerDown) {
            phase = 2;
        }
        handle_pointer(phase, info.ptPixelLocation, pressure, contact);
        return 0;
    }
    case WM_POINTERCAPTURECHANGED:
        if (app.pointerDown) {
            handle_pointer(2, (POINT){0, 0}, -1.0f, false);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        SetCapture(window);
        POINT screen = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
        ClientToScreen(window, &screen);
        handle_pointer(0, screen, -1.0f, true);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT screen = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
        ClientToScreen(window, &screen);
        handle_pointer(1, screen, -1.0f, (wparam & MK_LBUTTON) != 0);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT screen = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
        ClientToScreen(window, &screen);
        ReleaseCapture();
        handle_pointer(2, screen, -1.0f, false);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (app.pointerDown && (HWND)lparam != window) {
            handle_pointer(2, (POINT){0, 0}, -1.0f, false);
        }
        return 0;
    case WM_KEYDOWN:
        handle_key(wparam);
        return 0;
    case WM_CHAR:
        if (controller_text_input(&app.controller, (wchar_t)wparam)) {
            after_pointer_event();
        }
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, app.controller.tool == TOOL_SELECT
                                        ? IDC_ARROW
                                        : (app.controller.tool == TOOL_TEXT ? IDC_IBEAM : IDC_CROSS)));
        return TRUE;
    case WM_MOUSEACTIVATE:
        return app.drawing ? MA_ACTIVATE : MA_NOACTIVATE;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/**
 * @brief Re-reads monitors after a display change and repositions what depends on them.
 */
static void handle_display_change(void)
{
    refresh_monitors();
    ui_place_defaults(app.ui, app.monitors[app.config.uiMonitor].work, false);
    if (app.overlayReady) {
        controller_cancel(&app.controller);
        app_apply_state();
    }
}

/**
 * @brief Re-presents everything after sleep or an unlocked session so nothing stays stale or buried.
 */
static void handle_resume(void)
{
    if (app.overlayReady) {
        SetWindowPos(app.overlay.window, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        render_everything();
    }
    ui_raise(app.ui);
}

/**
 * @brief Builds the state the tray menu reflects.
 * @param state Receives the state.
 */
static void build_tray_state(TrayState *state)
{
    memset(state, 0, sizeof *state);
    state->drawing = app.drawing;
    state->whiteboard = app.whiteboard;
    state->monitorCount = app.monitorCount;
    state->drawMonitor = app.config.drawMonitor;
    state->uiMonitor = app.config.uiMonitor;
    state->hideUiFromCapture = app.config.ui.excludeFromCapture;
    state->autoHideUi = app.config.ui.autoHide;
    state->startWithWindows = autostart_get();
    state->learning = app.learning;
}

/**
 * @brief Loads the configuration file and applies it everywhere.
 */
static void reload_config(void)
{
    wchar_t errors[256] = L"";
    bool existed = config_load(&app.config, app.configPath, errors, 256);
    if (!existed) {
        config_save(&app.config, app.configPath);
    }
    refresh_monitors();
    app.controller.style.stroke = STROKE_PALETTE[app.config.defaultStrokeIndex];
    app.controller.style.fill = FILL_PALETTE[app.config.defaultFillIndex];
    app.controller.style.width = WIDTH_PRESETS[app.config.defaultWidthIndex];
    app.controller.toolLocked = app.config.toolLock;
    if (errors[0]) {
        notify(L"Configuration warnings", errors);
    }
    apply_hotkeys();
}

/**
 * @brief Handles a command chosen in the tray menu.
 * @param event The chosen command.
 */
static void handle_tray_event(const TrayEvent *event)
{
    switch (event->command) {
    case TRAY_TOGGLE_DRAWING:
        set_drawing(!app.drawing);
        break;
    case TRAY_SET_DESKTOP_MODE:
        set_whiteboard(false);
        break;
    case TRAY_SET_WHITEBOARD_MODE:
        set_whiteboard(true);
        break;
    case TRAY_SET_DRAW_MONITOR:
        app.config.drawMonitor = event->value;
        config_save(&app.config, app.configPath);
        controller_cancel(&app.controller);
        app_apply_state();
        break;
    case TRAY_SET_UI_MONITOR:
        app.config.uiMonitor = event->value;
        for (int i = 0; i < PANEL_COUNT; i++) {
            app.config.ui.placement[i].set = false;
        }
        config_save(&app.config, app.configPath);
        ui_place_defaults(app.ui, app.monitors[app.config.uiMonitor].work, true);
        break;
    case TRAY_TOGGLE_HIDE_UI_FROM_CAPTURE:
        app.config.ui.excludeFromCapture = !app.config.ui.excludeFromCapture;
        ui_set_capture_excluded(app.ui, app.config.ui.excludeFromCapture);
        config_save(&app.config, app.configPath);
        break;
    case TRAY_TOGGLE_AUTO_HIDE_UI:
        app.config.ui.autoHide = !app.config.ui.autoHide;
        config_save(&app.config, app.configPath);
        ui_apply_settings(app.ui, &app.config.ui);
        break;
    case TRAY_LEARN_ACTION:
        if (hotkeys_learn_begin(app.hotkeys, event->action)) {
            app.learning = true;
            notify(L"Press a button", L"Press the pen button the way you want to use it. Esc cancels.");
        }
        break;
    case TRAY_CANCEL_LEARN:
        hotkeys_learn_cancel(app.hotkeys);
        break;
    case TRAY_RELOAD_CONFIG:
        reload_config();
        app_sync_ui();
        break;
    case TRAY_OPEN_CONFIG:
        ShellExecuteW(NULL, L"open", app.configPath, NULL, NULL, SW_SHOWNORMAL);
        break;
    case TRAY_COPY_PNG:
        copy_drawing_png();
        break;
    case TRAY_TOGGLE_START_WITH_WINDOWS:
        autostart_set(!autostart_get());
        break;
    case TRAY_QUIT:
        app.quitting = true;
        PostQuitMessage(0);
        break;
    case TRAY_NONE:
        break;
    }
}

/**
 * @brief Message window procedure: hotkeys, timers, tray, display and power events.
 * @param window Message window.
 * @param message Window message.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return Message result.
 */
static LRESULT CALLBACK message_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_HOTKEY:
        if (wparam == EMERGENCY_HOTKEY_ID) {
            perform_action(ACTION_EMERGENCY_EXIT);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wparam == TIMER_ANIMATE) {
            bool animating = controller_animate(&app.controller, now_ms());
            if (!animating) {
                KillTimer(window, TIMER_ANIMATE);
            }
            app_flush();
            if (!animating) {
                trim_working_set();
            }
            return 0;
        }
        if (wparam == TIMER_FADE_WAIT) {
            KillTimer(window, TIMER_FADE_WAIT);
            app.fadeStartMs = now_ms();
            SetTimer(window, TIMER_FADE_STEP, ANIMATION_INTERVAL_MS, NULL);
            return 0;
        }
        if (wparam == TIMER_FADE_STEP) {
            fade_step();
            return 0;
        }
        break;
    case WM_APP_TRAY: {
        TrayState state;
        TrayEvent event;
        build_tray_state(&state);
        if (tray_handle_message(app.tray, lparam, &state, &event)) {
            handle_tray_event(&event);
        }
        return 0;
    }
    case WM_APP_ACTIVATE:
        set_drawing(!app.drawing);
        return 0;
    case WM_DISPLAYCHANGE:
        handle_display_change();
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wparam == WTS_SESSION_UNLOCK) {
            handle_resume();
        }
        return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMRESUMEAUTOMATIC) {
            handle_display_change();
            handle_resume();
        }
        return TRUE;
    case WM_DESTROY:
        return 0;
    default:
        if (message == app.taskbarCreatedMessage && app.tray) {
            tray_restore(app.tray);
            return 0;
        }
        break;
    }
    if (app.hotkeys && hotkeys_handle_message(app.hotkeys, message, wparam, lparam)) {
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/**
 * @brief Hides every window when the process crashes so input is never trapped.
 * @param info Exception information, unused.
 * @return EXCEPTION_EXECUTE_HANDLER to terminate after cleanup.
 */
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *info)
{
    (void)info;
    if (app.overlayReady && app.overlay.window) {
        ShowWindow(app.overlay.window, SW_HIDE);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

/**
 * @brief Builds the configuration file path next to the executable.
 * @param path Output buffer of MAX_PATH characters.
 */
static void build_config_path(wchar_t *path)
{
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *slash = wcsrchr(path, L'\\');
    if (slash) {
        wcscpy(slash + 1, L"ink.ini");
    }
}

/**
 * @brief Wakes the already running instance instead of starting a second one.
 * @return true when another instance exists.
 */
static bool signal_existing_instance(void)
{
    HANDLE mutex = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_MUTEX);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(MESSAGE_CLASS, NULL);
        if (other) {
            PostMessageW(other, WM_APP_ACTIVATE, 0, 0);
        }
        CloseHandle(mutex);
        return true;
    }
    return false;
}

/**
 * @brief Creates the hidden message window.
 * @return true on success.
 */
static bool create_message_window(void)
{
    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof windowClass;
    windowClass.lpfnWndProc = message_procedure;
    windowClass.hInstance = app.instance;
    windowClass.lpszClassName = MESSAGE_CLASS;
    RegisterClassExW(&windowClass);
    app.messageWindow = CreateWindowExW(0, MESSAGE_CLASS, L"Ink", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
                                        app.instance, NULL);
    return app.messageWindow != NULL;
}

int app_run(HINSTANCE instance)
{
    if (signal_existing_instance()) {
        return 0;
    }
    SetUnhandledExceptionFilter(crash_filter);
    memset(&app, 0, sizeof app);
    app.instance = instance;
    app.previousTool = TOOL_PEN;
    app.taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    if (!create_message_window()) {
        return 1;
    }
    scene_init(&app.scenes[0]);
    scene_init(&app.scenes[1]);
    controller_init(&app.controller);
    controller_attach(&app.controller, &app.scenes[0], NULL);
    config_defaults(&app.config);
    build_config_path(app.configPath);

    app.hotkeys = hotkeys_create(app.messageWindow, on_hotkey_action, on_learn_result, NULL);
    RegisterHotKey(app.messageWindow, EMERGENCY_HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_ESCAPE);

    reload_config();
    controller_set_tool(&app.controller, app.config.defaultTool);
    app.ui = ui_create(instance, on_ui_event, NULL, &app.config.ui);
    if (app.ui) {
        ui_set_capture_excluded(app.ui, app.config.ui.excludeFromCapture);
        ui_place_defaults(app.ui, app.monitors[app.config.uiMonitor].work, false);
        ui_set_visible(app.ui, true, false);
        app_sync_ui();
    }
    create_tray();
    WTSRegisterSessionNotification(app.messageWindow, NOTIFY_FOR_THIS_SESSION);

    trim_working_set();

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    WTSUnRegisterSessionNotification(app.messageWindow);
    UnregisterHotKey(app.messageWindow, EMERGENCY_HOTKEY_ID);
    destroy_overlay();
    ui_destroy(app.ui);
    hotkeys_destroy(app.hotkeys);
    tray_destroy(app.tray);
    controller_destroy(&app.controller);
    scene_destroy(&app.scenes[0]);
    scene_destroy(&app.scenes[1]);
    text_shutdown();
    DestroyWindow(app.messageWindow);
    return 0;
}
