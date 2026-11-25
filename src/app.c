/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app.h"

#include <stdio.h>
#include <string.h>
#include <commdlg.h>
#include <wtsapi32.h>

#include "config.h"
#include "controller.h"
#include "version.h"
#include "hotkeys.h"
#include "jobs.h"
#include "log.h"
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
#define TIMER_TRAY_RETRY 4
#define TIMER_RESUME_HOTKEYS 5
#define SEND_KEY_RESUME_MS 400
#define TRAY_RETRY_FIRST_MS 2000
#define TRAY_RETRY_LIMIT_MS 30000
#define JOBS_SHUTDOWN_WAIT_MS 3000
#define EMERGENCY_CHOICES 3
#define ANIMATION_INTERVAL_MS 16
#define FADE_DURATION_MS 400
#define DISPLAY_LIMIT TRAY_MAX_MONITORS
#define MESSAGE_CLASS L"PenPaneMessageWindow"
#define SINGLE_INSTANCE_MUTEX L"Local\\PenPaneOverlaySingleInstance"
#define SCRATCH_KEEP_BYTES (256 * 1024)
#define PENDING_NOTICE_LIMIT 4

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
    wchar_t logPath[MAX_PATH];
    Scene scenes[2];
    bool whiteboard;
    bool drawing;
    bool drawingsHidden;
    unsigned dialogs;
    float pickerHue;
    float pickerSat;
    float pickerVal;
    bool overlayReady;
    bool learning;
    unsigned learnTool;
    bool learnSilent;
    int settingsTab;
    bool quitting;
    bool configProtected;
    bool emergencyRegistered;
    HPOWERNOTIFY powerNotification;
    UINT trayRetryMs;
    UINT32 activePointer;
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

/** @brief Brings the windows, surface and interface in line with the drawing and mode flags. */
static void app_apply_state(void);

/** @brief Repaints the dirty regions of the overlay and presents them. */
static void app_flush(void);

/** @brief Shows a notification, or queues it until the tray icon exists. */
static void notify(const wchar_t *title, const wchar_t *text);

/** @brief Puts every option back to its default. */
static void reset_options(void);

/** @brief Puts the toolbar and the panels back at their default places. */
static void reset_positions(void);

/** @brief Pushes the current tool, style and history state to the floating interface. */
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
 * @brief Backdrop color for the current mode.
 * @return Transparent, or the opaque whiteboard color.
 */
static Color backdrop_color(void)
{
    return app.whiteboard ? COLOR_CANVAS_DARK : 0;
}

/**
 * @brief Tells whether drawing runs with a separate input window and a click-through visual window.
 * @return true for drawing on the desktop; whiteboard keeps one opaque window.
 */
static bool two_window_mode(void)
{
    return app.drawing && !app.whiteboard;
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
 * @brief Re-reads the monitor layout.
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
}

/**
 * @brief Maps a configured monitor index to one that exists, without touching the configuration.
 * @param configured Index from the configuration.
 * @return The index when a monitor with that number is connected, otherwise 0.
 */
static int usable_monitor(int configured)
{
    return configured >= 0 && configured < app.monitorCount ? configured : 0;
}

/**
 * @brief Monitor the overlay currently draws on.
 * @return Index into the monitor list.
 */
static int draw_monitor(void)
{
    return usable_monitor(app.config.drawMonitor);
}

/**
 * @brief Monitor that hosts the tool panels.
 * @return Index into the monitor list.
 */
static int panel_monitor(void)
{
    return usable_monitor(app.config.uiMonitor);
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
    HWND target = overlay_focus_window(&app.overlay);
    HWND foreground = GetForegroundWindow();
    if (foreground == target) {
        return;
    }
    if (foreground != app.overlay.window) {
        app.previousForeground = foreground;
    }
    DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
    DWORD thisThread = GetCurrentThreadId();
    bool attached = foregroundThread && foregroundThread != thisThread &&
                    AttachThreadInput(thisThread, foregroundThread, TRUE);
    SetForegroundWindow(target);
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
        GetForegroundWindow() == overlay_focus_window(&app.overlay)) {
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

/**
 * @brief Creates the overlay window on the monitor chosen for drawing.
 * @return true on success.
 */
static bool create_overlay(void)
{
    const Monitor *monitor = &app.monitors[draw_monitor()];
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
    case TOOL_HIGHLIGHTER:
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_OPACITY;
    case TOOL_RECTANGLE:
        type = ELEMENT_RECTANGLE;
        break;
    case TOOL_ELLIPSE:
        type = ELEMENT_ELLIPSE;
        break;
    case TOOL_DIAMOND:
        type = ELEMENT_DIAMOND;
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
    case TOOL_LASER:
        return UI_SECTION_LASER;
    default:
        return 0;
    }
    switch (type) {
    case ELEMENT_RECTANGLE:
        return UI_SECTION_STROKE | UI_SECTION_FILL | UI_SECTION_FILL_STYLE | UI_SECTION_WIDTH |
               UI_SECTION_STROKE_STYLE | UI_SECTION_CORNERS | UI_SECTION_OPACITY;
    case ELEMENT_ELLIPSE:
    case ELEMENT_DIAMOND:
        return UI_SECTION_STROKE | UI_SECTION_FILL | UI_SECTION_FILL_STYLE | UI_SECTION_WIDTH |
               UI_SECTION_STROKE_STYLE | UI_SECTION_OPACITY;
    case ELEMENT_LINE:
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_STROKE_STYLE | UI_SECTION_ARROWHEADS |
               UI_SECTION_OPACITY;
    case ELEMENT_PEN:
        if (selection && app.controller.tool == TOOL_SELECT && selection->style.highlighter) {
            return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_OPACITY;
        }
        return UI_SECTION_STROKE | UI_SECTION_WIDTH | UI_SECTION_PRESSURE | UI_SECTION_OPACITY;
    case ELEMENT_TEXT:
        return UI_SECTION_STROKE | UI_SECTION_FONT | UI_SECTION_FONT_SIZE | UI_SECTION_TEXT_ALIGN |
               UI_SECTION_OPACITY;
    }
    return 0;
}

/**
 * @brief Writes the extra shortcut of a tool, for example "F15" or "Ctrl+F9 x2", or an empty string.
 * @param tool Tool whose binding is wanted; the first binding of its tool_* action is used.
 * @param buffer Output buffer.
 * @param size Buffer size in characters.
 */
static void tool_shortcut_text(Tool tool, wchar_t *buffer, int size)
{
    buffer[0] = L'\0';
    Action action = (Action)(ACTION_TOOL_SELECT + (int)tool);
    for (int i = 0; i < app.config.bindingCount; i++) {
        const Binding *binding = &app.config.bindings[i];
        if (binding->action != action) {
            continue;
        }
        static const wchar_t *const SUFFIX[] = {L"", L" x2", L" hold"};
        wchar_t trigger[24];
        trigger_to_text(binding->trigger, trigger, 24);
        _snwprintf(buffer, (size_t)size, L"%ls%ls", trigger, SUFFIX[binding->gesture]);
        buffer[size - 1] = L'\0';
        return;
    }
}

/**
 * @brief Pushes the current tool, style and history state to the floating interface.
 */
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
    model.laserPointer = app.controller.laserPointer;
    memcpy(model.toolOrder, app.config.toolOrder, sizeof model.toolOrder);
    model.hiddenTools = app.config.hiddenTools;
    model.drawingsHidden = app.drawingsHidden;
    model.autoHide = app.config.ui.autoHide;
    model.excludeCapture = app.config.ui.excludeFromCapture;
    model.dialogs = app.dialogs;
    model.pastel = app.config.palettePastel;
    model.debugLog = app.config.debugLog;
    model.exportBackground = app.config.exportBackground;
    model.exportFullScreen = app.config.exportFullScreen;
    model.learningTool = (int)app.learnTool - 1;
    model.settingsTab = app.settingsTab;
    for (int t = 0; t < TOOL_COUNT; t++) {
        tool_shortcut_text((Tool)t, model.toolShortcut[t], UI_SHORTCUT_CHARS);
    }
    memcpy(model.custom, app.config.customColors, sizeof model.custom);
    model.customCount = app.config.customCount;
    if (color_from_hsv(app.pickerHue, app.pickerSat, app.pickerVal) != (model.style.stroke | 0xFF000000u)) {
        color_to_hsv(model.style.stroke, &app.pickerHue, &app.pickerSat, &app.pickerVal);
    }
    model.pickerHue = app.pickerHue;
    model.pickerSat = app.pickerSat;
    model.pickerVal = app.pickerVal;
    model.canUndo = scene_can_undo(active_scene());
    model.canRedo = scene_can_redo(active_scene());
    model.whiteboard = app.whiteboard;
    model.drawing = app.drawing;
    ui_update(app.ui, &model);
}

/**
 * @brief Union of scene element bounds and transient drawing bounds.
 * @return Dirty region in monitor coordinates, empty when nothing is visible.
 */
static Dirty annotation_extent(void)
{
    const Scene *scene = active_scene();
    Dirty extent;
    dirty_reset(&extent);
    for (int i = 0; i < scene->count; i++) {
        dirty_add(&extent, element_bounds(&scene->items[i]));
    }
    controller_add_transient_bounds(&app.controller, &extent);
    return extent;
}

/**
 * @brief Surface rectangle the overlay needs: the whole monitor while drawing or on the whiteboard, the
 * annotation extent otherwise.
 *
 * Drawing keeps a fixed full-monitor surface: growing it during a stroke moved and reallocated the
 * layered window and made pen strokes visibly stutter.
 * @return Rectangle in monitor coordinates; a single pixel when nothing is visible.
 */
static RECT wanted_surface(void)
{
    const Monitor *monitor = &app.monitors[draw_monitor()];
    RECT full = {0, 0, monitor->bounds.right - monitor->bounds.left,
                 monitor->bounds.bottom - monitor->bounds.top};
    if (app.whiteboard || app.drawing) {
        return full;
    }
    Dirty extent = annotation_extent();
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

/** @brief true while TIMER_ANIMATE runs; it must not be re-armed, or steady pointer input keeps resetting it.
 */
static bool animationTimerOn;

/**
 * @brief Starts the trail animation timer while a laser or eraser trail is visible.
 */
static void update_animation_timer(void)
{
    if (!animationTimerOn && (app.controller.laser.count > 0 || app.controller.eraser.count > 0)) {
        animationTimerOn = SetTimer(app.messageWindow, TIMER_ANIMATE, ANIMATION_INTERVAL_MS, NULL) != 0;
    }
}

/**
 * @brief Repaints the dirty regions of the overlay and presents them, resizing the surface outside whiteboard
 * when needed.
 */
static void app_flush(void)
{
    Dirty repaint;
    Dirty direct;
    controller_take_dirty(&app.controller, &repaint, &direct);
    if (!app.overlayReady) {
        return;
    }
    if (!app.whiteboard && (app.drawing || !repaint.empty)) {
        RECT wanted = wanted_surface();
        RECT current = canvas_area(&app.overlay.canvas);
        if (!EqualRect(&wanted, &current)) {
            if (overlay_set_surface(&app.overlay, wanted)) {
                render_everything();
                update_animation_timer();
                return;
            }
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

/**
 * @brief Brings windows, surface, focus and interface in line with the drawing and mode flags.
 *
 * The monitor list is read again first so that a layout change that was never announced still
 * leaves the overlay on a monitor that exists.
 */
static void app_apply_state(void)
{
    refresh_monitors();
    Scene *scene = active_scene();
    bool wantVisible = app.drawing || (!app.whiteboard && scene->count > 0 && !app.drawingsHidden);
    LOG("state: drawing=%d whiteboard=%d hidden=%d items=%d overlay=%d", app.drawing, app.whiteboard,
        app.drawingsHidden, scene->count, wantVisible);
    cancel_fade();
    if (!wantVisible) {
        if (app.overlayReady) {
            restore_focus();
        }
        destroy_overlay();
    } else {
        if (!app.overlayReady && !create_overlay()) {
            LOG("overlay window could not be created (error %lu)", GetLastError());
            return;
        }
        if (!app.drawing) {
            restore_focus();
        }
        overlay_set_bounds(&app.overlay, app.monitors[draw_monitor()].bounds);
        if (!overlay_set_input_window(&app.overlay, two_window_mode(), app.instance) ||
            !overlay_set_surface(&app.overlay, wanted_surface())) {
            LOG("overlay input window or surface could not be set up (error %lu)", GetLastError());
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
        } else if (app.config.autoFadeSeconds > 0 && scene->count > 0) {
            SetTimer(app.messageWindow, TIMER_FADE_WAIT, (UINT)app.config.autoFadeSeconds * 1000, NULL);
        }
        overlay_raise(&app.overlay);
    }
    if (app.ui) {
        ui_set_visible(app.ui, true, app.drawing || app.drawingsHidden);
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
        controller_deselect(&app.controller);
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
    if (drawing) {
        app.drawingsHidden = false;
    }
    app.drawing = drawing;
    app_apply_state();
}

/**
 * @brief Hides every drawing so the desktop can be used, or shows them again; nothing is deleted.
 *
 * Hiding ends drawing mode but the panels stay up so the drawings can be brought back; showing
 * them again goes back to drawing mode, so the panels stay up in both states.
 */
static void toggle_drawings(void)
{
    if (app.whiteboard) {
        return;
    }
    if (app.drawingsHidden) {
        app.drawingsHidden = false;
        app.drawing = true;
    } else {
        controller_cancel(&app.controller);
        app_flush();
        app.drawingsHidden = true;
        app.drawing = false;
    }
    app_apply_state();
}

/**
 * @brief Opens or closes one dialog, leaving the others as they are, and pushes the change to the UI.
 * @param dialog Dialog island: PANEL_SETTINGS, PANEL_CONFIRM or PANEL_COLOR.
 * @param open true to show it.
 */
static void set_dialog(PanelId dialog, bool open)
{
    if (open) {
        app.dialogs |= UI_DIALOG_BIT(dialog);
    } else {
        app.dialogs &= ~UI_DIALOG_BIT(dialog);
    }
    app_sync_ui();
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
    app.drawingsHidden = false;
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
 * @brief Draws the export canvas over a screenshot of the desktop below it, leaving it fully opaque.
 *
 * The overlay windows and tool panels are left out of the screenshot while it is taken.
 * @param export Rendered drawing with premultiplied pixels; replaced by the combined image.
 * @param screen Area of the export canvas in virtual-desktop coordinates.
 * @return false when the screenshot failed; the canvas is untouched then.
 */
static bool composite_over_desktop(Canvas *export, RECT screen)
{
    int width = export->width;
    int height = export->height;
    BITMAPINFO info = {0};
    info.bmiHeader = canvas_dib_header(width, -height);
    HDC screenDc = GetDC(NULL);
    HDC memoryDc = screenDc ? CreateCompatibleDC(screenDc) : NULL;
    uint32_t *bits = NULL;
    HBITMAP bitmap =
        memoryDc ? CreateDIBSection(screenDc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0) : NULL;
    bool ok = false;
    if (bitmap && bits) {
        HGDIOBJ previous = SelectObject(memoryDc, bitmap);
        if (app.overlayReady) {
            overlay_set_visible(&app.overlay, false);
        }
        if (app.ui) {
            ui_set_visible(app.ui, false, false);
        }
        Sleep(150);
        ok = BitBlt(memoryDc, 0, 0, width, height, screenDc, screen.left, screen.top, SRCCOPY) != 0;
        if (app.overlayReady) {
            overlay_set_visible(&app.overlay, true);
            overlay_raise(&app.overlay);
        }
        if (app.ui) {
            ui_set_visible(app.ui, true, app.drawing || app.drawingsHidden);
            ui_raise(app.ui);
        }
        if (app.overlayReady && app.drawing) {
            focus_overlay();
        }
        SelectObject(memoryDc, previous);
        GdiFlush();
    }
    if (ok) {
        for (size_t i = 0; i < (size_t)width * (size_t)height; i++) {
            uint32_t top = export->pixels[i];
            uint32_t rest = 255 - (top >> 24);
            uint32_t below = bits[i];
            uint32_t red = ((top >> 16) & 0xFF) + (((below >> 16) & 0xFF) * rest + 127) / 255;
            uint32_t green = ((top >> 8) & 0xFF) + (((below >> 8) & 0xFF) * rest + 127) / 255;
            uint32_t blue = (top & 0xFF) + ((below & 0xFF) * rest + 127) / 255;
            export->pixels[i] = 0xFF000000u | (red > 255 ? 255 : red) << 16 |
                                (green > 255 ? 255 : green) << 8 | (blue > 255 ? 255 : blue);
        }
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (memoryDc) {
        DeleteDC(memoryDc);
    }
    if (screenDc) {
        ReleaseDC(NULL, screenDc);
    }
    return ok;
}

/**
 * @brief Renders the active drawing into a canvas that covers only the exported area.
 *
 * The area is the painted extent, or the whole draw screen when "export the full screen" is on. On the
 * desktop, "add desktop to exports" puts the drawing over a screenshot of what is behind it.
 * @param export Receives the canvas; the caller owns it on success.
 * @param local Receives the exported rectangle in the canvas' own pixels.
 * @param opaque Receives whether every exported pixel is opaque.
 * @return false when there is nothing to export or memory ran out.
 */
static bool render_export(Canvas *export, RECT *local, bool *opaque)
{
    Scene *scene = active_scene();
    RECT region = {0, 0, 0, 0};
    const Monitor *monitor = &app.monitors[draw_monitor()];
    int width = monitor->bounds.right - monitor->bounds.left;
    int height = monitor->bounds.bottom - monitor->bounds.top;
    if (app.config.exportFullScreen) {
        region.right = width;
        region.bottom = height;
    } else {
        Dirty extent;
        dirty_reset(&extent);
        for (int i = 0; i < scene->count; i++) {
            dirty_add(&extent, element_bounds(&scene->items[i]));
        }
        if (extent.empty) {
            return false;
        }
        region = extent.rect;
    }
    RECT bounds = {0, 0, width, height};
    IntersectRect(&region, &region, &bounds);
    int exportWidth = region.right - region.left;
    int exportHeight = region.bottom - region.top;
    if (exportWidth <= 0 || exportHeight <= 0 || !canvas_create(export, exportWidth, exportHeight)) {
        return false;
    }
    export->originX = region.left;
    export->originY = region.top;
    render_scene(export, scene, &region, backdrop_color());
    *opaque = app.whiteboard;
    if (!app.whiteboard && app.config.exportBackground) {
        RECT screen = {monitor->bounds.left + region.left, monitor->bounds.top + region.top,
                       monitor->bounds.left + region.right, monitor->bounds.top + region.bottom};
        if (composite_over_desktop(export, screen)) {
            *opaque = true;
        } else {
            notify(L"Export", L"The desktop could not be captured; exported the drawing alone.");
        }
    }
    SetRect(local, 0, 0, exportWidth, exportHeight);
    return true;
}

/**
 * @brief Copies the active drawing to the clipboard as PNG.
 */
static void copy_drawing_png(void)
{
    Canvas export;
    RECT local;
    bool opaque;
    if (!render_export(&export, &local, &opaque)) {
        return;
    }
    if (!jobs_copy_region(app.messageWindow, &export, local, opaque)) {
        canvas_destroy(&export);
        notify(L"Export", L"Another export is still running; try again in a moment.");
    }
}

/**
 * @brief Asks where to save a PNG with the system save dialog, loaded only when needed.
 * @param path Receives the chosen path.
 * @param size Capacity of path in characters.
 * @return false when the user cancelled or the dialog is unavailable.
 */
static bool choose_png_path(wchar_t *path, DWORD size)
{
    typedef BOOL(WINAPI * SaveDialog)(LPOPENFILENAMEW);
    HMODULE library = LoadLibraryW(L"comdlg32.dll");
    SaveDialog dialog = library ? (SaveDialog)(void *)GetProcAddress(library, "GetSaveFileNameW") : NULL;
    bool chosen = false;
    if (dialog) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        swprintf(path, size, L"PenPane %04d-%02d-%02d %02d-%02d-%02d.png", now.wYear, now.wMonth, now.wDay,
                 now.wHour, now.wMinute, now.wSecond);
        OPENFILENAMEW dialogInfo = {0};
        dialogInfo.lStructSize = sizeof dialogInfo;
        dialogInfo.hwndOwner = app.overlayReady ? overlay_focus_window(&app.overlay) : app.messageWindow;
        dialogInfo.lpstrFilter = L"PNG image (*.png)\0*.png\0";
        dialogInfo.lpstrFile = path;
        dialogInfo.nMaxFile = size;
        dialogInfo.lpstrDefExt = L"png";
        dialogInfo.lpstrTitle = L"Save drawing as PNG";
        dialogInfo.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        chosen = dialog(&dialogInfo) != FALSE;
    }
    if (library) {
        FreeLibrary(library);
    }
    return chosen;
}

/**
 * @brief Saves the active drawing as a PNG file chosen by the user.
 */
static void save_drawing_png(void)
{
    Canvas export;
    RECT local;
    bool opaque;
    if (!render_export(&export, &local, &opaque)) {
        return;
    }
    wchar_t path[MAX_PATH];
    bool chosen = choose_png_path(path, ARRAYSIZE(path));
    if (app.overlayReady) {
        overlay_raise(&app.overlay);
    }
    if (app.ui) {
        ui_raise(app.ui);
    }
    if (!chosen || !jobs_save_region(&export, local, opaque, path)) {
        canvas_destroy(&export);
        if (chosen) {
            notify(L"Export", L"The drawing was not saved: another export is still running.");
        }
    }
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
 * @brief Applies a style change that belongs to a continuous drag, without adding an undo step.
 *
 * The edits are recorded as one step when the drag ends (see finish_style_drag).
 * @param style New style.
 */
static void preview_style(Style style)
{
    app.controller.style = style;
    if (controller_selection(&app.controller)) {
        controller_style_preview(&app.controller, &style);
    }
    app_flush();
    app_sync_ui();
}

/**
 * @brief Ends a continuous style drag and records it as a single undo step.
 */
static void finish_style_drag(void)
{
    controller_style_commit(&app.controller);
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
    case ACTION_TOGGLE_DRAWINGS:
        toggle_drawings();
        break;
    case ACTION_ASK_CLEAR:
        set_dialog(PANEL_CONFIRM, true);
        break;
    case ACTION_OPEN_SETTINGS:
        set_dialog(PANEL_SETTINGS, (app.dialogs & UI_DIALOG_BIT(PANEL_SETTINGS)) == 0);
        break;
    case ACTION_TOOL_SELECT:
    case ACTION_TOOL_RECTANGLE:
    case ACTION_TOOL_ELLIPSE:
    case ACTION_TOOL_LINE:
    case ACTION_TOOL_PEN:
    case ACTION_TOOL_LASER:
    case ACTION_TOOL_TEXT:
    case ACTION_TOOL_ERASER:
    case ACTION_TOOL_DIAMOND:
    case ACTION_TOOL_HIGHLIGHTER: {
        static const Tool MAP[] = {TOOL_SELECT, TOOL_RECTANGLE, TOOL_ELLIPSE, TOOL_LINE,    TOOL_PEN,
                                   TOOL_LASER,  TOOL_TEXT,      TOOL_ERASER,  TOOL_DIAMOND, TOOL_HIGHLIGHTER};
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
        int index = style_width_index(&style);
        style.width = style_width_preset(&style, (index + 1) % WIDTH_PRESET_COUNT);
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
    case ACTION_SAVE_PNG:
        save_drawing_png();
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
    LOG("notice: %ls - %ls", title, text);
    if (app.tray && tray_notify(app.tray, title, text)) {
        return;
    }
    if (app.pendingCount == PENDING_NOTICE_LIMIT) {
        memmove(app.pending, app.pending + 1, sizeof(Notice) * (PENDING_NOTICE_LIMIT - 1));
        app.pendingCount--;
    }
    Notice *notice = &app.pending[app.pendingCount++];
    memset(notice, 0, sizeof *notice);
    wcsncpy(notice->title, title, ARRAYSIZE(notice->title) - 1);
    wcsncpy(notice->text, text, ARRAYSIZE(notice->text) - 1);
}

/**
 * @brief Delivers queued notifications once the tray icon is visible, keeping those the shell refuses.
 */
static void flush_notices(void)
{
    while (app.pendingCount > 0 && tray_is_registered(app.tray)) {
        const Notice *first = &app.pending[0];
        if (!tray_notify(app.tray, first->title, first->text)) {
            return;
        }
        app.pendingCount--;
        memmove(app.pending, app.pending + 1, sizeof(Notice) * (size_t)app.pendingCount);
    }
}

/**
 * @brief Arms the one-shot timer that retries the tray icon, backing off up to a limit.
 */
static void schedule_tray_retry(void)
{
    if (app.trayRetryMs == 0) {
        app.trayRetryMs = TRAY_RETRY_FIRST_MS;
    }
    SetTimer(app.messageWindow, TIMER_TRAY_RETRY, app.trayRetryMs, NULL);
    app.trayRetryMs = app.trayRetryMs * 2 > TRAY_RETRY_LIMIT_MS ? TRAY_RETRY_LIMIT_MS : app.trayRetryMs * 2;
}

/**
 * @brief Registers the tray icon again after the taskbar was recreated or a retry is due.
 * @param force true to drop the registration first, as after "TaskbarCreated".
 */
static void restore_tray(bool force)
{
    if (!app.tray) {
        app.tray = tray_create(app.messageWindow, WM_APP_TRAY);
    }
    bool visible = force ? tray_restore(app.tray) : tray_retry(app.tray);
    if (visible) {
        KillTimer(app.messageWindow, TIMER_TRAY_RETRY);
        app.trayRetryMs = 0;
        flush_notices();
    } else {
        schedule_tray_retry();
    }
}

/**
 * @brief Creates the tray icon, retrying later when the shell is not ready, and delivers queued
 * notifications.
 */
static void create_tray(void)
{
    app.tray = tray_create(app.messageWindow, WM_APP_TRAY);
    if (tray_is_registered(app.tray)) {
        flush_notices();
    } else {
        schedule_tray_retry();
    }
}

/**
 * @brief Persists the configuration without blocking the message loop; skipped while the file is protected.
 */
static void save_config(void)
{
    if (app.configProtected || app.configPath[0] == L'\0') {
        return;
    }
    if (!jobs_save_config(&app.config, app.configPath)) {
        notify(L"Settings", L"The settings could not be saved.");
    }
}

/**
 * @brief Registers the emergency exit, falling back to other combinations when the first is taken.
 *
 * Ctrl+Alt+Shift+Esc is tried with auto-repeat suppressed, then without the flag, and finally
 * Ctrl+Alt+Shift+F12. The user is told when the fallback is active or when nothing could be registered.
 */
static void register_emergency_hotkey(void)
{
    static const struct {
        UINT modifiers;
        UINT key;
    } CHOICES[EMERGENCY_CHOICES] = {
        {MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_ESCAPE},
        {MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_ESCAPE},
        {MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_F12},
    };
    for (int i = 0; i < EMERGENCY_CHOICES; i++) {
        if (RegisterHotKey(app.messageWindow, EMERGENCY_HOTKEY_ID, CHOICES[i].modifiers, CHOICES[i].key)) {
            app.emergencyRegistered = true;
            if (CHOICES[i].key != VK_ESCAPE) {
                notify(L"Emergency exit changed",
                       L"Another program uses Ctrl+Alt+Shift+Esc. Press Ctrl+Alt+Shift+F12 to quit PenPane.");
            }
            return;
        }
    }
    notify(L"Emergency exit unavailable", L"Ctrl+Alt+Shift+Esc and Ctrl+Alt+Shift+F12 are both taken by "
                                          L"other programs. Use the tray menu to quit PenPane.");
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
 * @brief Types a key into the window that has the keyboard focus, for the key box of a pen driver.
 *
 * PenPane's own hotkeys are released for a moment, because a registered hotkey would take the key
 * (F13 for instance) before the focused window sees it.
 * @param virtualKey Virtual-key code to press and release.
 */
static void send_key(WORD virtualKey)
{
    hotkeys_suspend(app.hotkeys);
    INPUT input[2];
    memset(input, 0, sizeof input);
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = virtualKey;
    input[1] = input[0];
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    UINT sent = SendInput(2, input, sizeof(INPUT));
    LOG("send key 0x%02X sent=%u", (unsigned)virtualKey, sent);
    SetTimer(app.messageWindow, TIMER_RESUME_HOTKEYS, SEND_KEY_RESUME_MS, NULL);
}

/**
 * @brief Receives the result of learn mode and stores the new binding.
 * @param user Unused.
 * @param action Action being learned.
 * @param binding Captured binding.
 * @param outcome How learn mode ended.
 */
static void on_learn_result(void *user, Action action, Binding binding, LearnOutcome outcome)
{
    (void)user;
    app.learning = false;
    bool replace = app.learnTool != 0;
    app.learnTool = 0;
    if (outcome == LEARN_REJECTED) {
        notify(L"Button not bound", L"A key that types text must be combined with Ctrl, Alt or Win.");
        app_sync_ui();
        return;
    }
    if (outcome != LEARN_CAPTURED) {
        if (!app.learnSilent) {
            notify(L"Learning cancelled", L"No button was bound.");
        }
        app_sync_ui();
        return;
    }
    binding.action = action;
    if (replace) {
        config_clear_action(&app.config, action);
    }
    if (!config_set_binding(&app.config, binding)) {
        notify(L"Button not bound", L"The list of bound buttons is full.");
        app_sync_ui();
        return;
    }
    save_config();
    apply_hotkeys();
    app_sync_ui();
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
        if (event->action == ACTION_CLEAR) {
            app.dialogs &= ~UI_DIALOG_BIT(PANEL_CONFIRM);
        }
        perform_action(event->action);
        break;
    case UI_EVENT_CLOSE_DIALOG:
        set_dialog(event->panel, false);
        break;
    case UI_EVENT_EDIT_END:
        if (app.drawing && app.overlayReady) {
            focus_overlay();
        }
        break;
    case UI_EVENT_SET_ARROWHEADS:
        style.arrowHeads = (ArrowHeads)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_CORNERS:
        style.corners = (Corners)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_FILL_STYLE:
        style.fillStyle = (FillStyle)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_TEXT_ALIGN:
        style.align = (TextAlign)event->index;
        change_style(style);
        break;
    case UI_EVENT_RESET_OPTIONS:
        reset_options();
        break;
    case UI_EVENT_RESET_POSITIONS:
        reset_positions();
        break;
    case UI_EVENT_SEND_KEY:
        send_key((WORD)event->index);
        break;
    case UI_EVENT_TOOL_SHORTCUT:
        if (app.learning) {
            hotkeys_learn_cancel(app.hotkeys);
        } else if (hotkeys_learn_begin(app.hotkeys, (Action)(ACTION_TOOL_SELECT + (int)event->tool))) {
            app.learning = true;
            app.learnTool = (unsigned)event->tool + 1;
            app_sync_ui();
        }
        break;
    case UI_EVENT_TOOL_SHORTCUT_KEY: {
        app.learnSilent = true;
        hotkeys_learn_cancel(app.hotkeys);
        app.learnSilent = false;
        Action action = (Action)(ACTION_TOOL_SELECT + (int)event->tool);
        Binding binding = {{TRIGGER_KEY, (unsigned)event->index, 0}, GESTURE_SINGLE, action};
        config_clear_action(&app.config, action);
        if (config_set_binding(&app.config, binding)) {
            save_config();
            apply_hotkeys();
            wchar_t text[160];
            describe_binding(binding, text, 160);
            notify(L"Button bound", text);
        }
        app_sync_ui();
        break;
    }
    case UI_EVENT_SETTINGS_TAB:
        app.settingsTab = event->index;
        app_sync_ui();
        break;
    case UI_EVENT_TOOL_SHORTCUT_CLEAR:
        config_clear_action(&app.config, (Action)(ACTION_TOOL_SELECT + (int)event->tool));
        save_config();
        apply_hotkeys();
        app_sync_ui();
        break;
    case UI_EVENT_EDIT_HEX:
        break;
    case UI_EVENT_TOOL_MOVE: {
        Tool *order = app.config.toolOrder;
        for (int i = 0; i < TOOL_COUNT; i++) {
            int other = i + event->index;
            if (order[i] == event->tool && other >= 0 && other < TOOL_COUNT) {
                Tool swap = order[i];
                order[i] = order[other];
                order[other] = swap;
                break;
            }
        }
        save_config();
        app_sync_ui();
        break;
    }
    case UI_EVENT_TOOL_VISIBLE: {
        unsigned mask = app.config.hiddenTools;
        mask = event->index != 0 ? (mask & ~(1u << event->tool)) : (mask | (1u << event->tool));
        if (mask != (1u << TOOL_COUNT) - 1u) {
            app.config.hiddenTools = mask;
            save_config();
        }
        app_sync_ui();
        break;
    }
    case UI_EVENT_SET_AUTO_HIDE:
        app.config.ui.autoHide = event->index != 0;
        save_config();
        ui_apply_settings(app.ui, &app.config.ui);
        app_sync_ui();
        break;
    case UI_EVENT_SET_EXCLUDE_CAPTURE:
        app.config.ui.excludeFromCapture = event->index != 0;
        save_config();
        ui_apply_settings(app.ui, &app.config.ui);
        app_sync_ui();
        break;
    case UI_EVENT_SELECT_TOOL:
        select_tool(event->tool);
        break;
    case UI_EVENT_SET_STROKE:
        style.stroke = STROKE_PALETTE[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_STROKE_COLOR:
        style.stroke = event->color;
        change_style(style);
        break;
    case UI_EVENT_SET_PASTEL:
        app.config.palettePastel = event->index != 0;
        save_config();
        app_sync_ui();
        break;
    case UI_EVENT_SET_EXPORT_BACKGROUND:
        app.config.exportBackground = event->index != 0;
        save_config();
        app_sync_ui();
        break;
    case UI_EVENT_SET_EXPORT_FULL_SCREEN:
        app.config.exportFullScreen = event->index != 0;
        save_config();
        app_sync_ui();
        break;
    case UI_EVENT_SET_DEBUG_LOG:
        app.config.debugLog = event->index != 0;
        log_set(app.config.debugLog, app.logPath);
        LOG("log on, PenPane %s", PENPANE_VERSION_STRING);
        save_config();
        app_sync_ui();
        break;
    case UI_EVENT_OPEN_PICKER:
        set_dialog(PANEL_COLOR, (app.dialogs & UI_DIALOG_BIT(PANEL_COLOR)) == 0);
        break;
    case UI_EVENT_PICK_HUE:
    case UI_EVENT_PICK_SV:
        if (event->kind == UI_EVENT_PICK_HUE) {
            app.pickerHue = event->value;
        } else {
            app.pickerSat = event->value;
            app.pickerVal = event->extra;
        }
        style.stroke = color_from_hsv(app.pickerHue, app.pickerSat, app.pickerVal);
        preview_style(style);
        break;
    case UI_EVENT_SET_FILL:
        style.fill = FILL_PALETTE[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_WIDTH:
        style.pressure = event->index >= WIDTH_PRESET_COUNT;
        if (!style.pressure) {
            style.width = style_width_preset(&style, event->index);
        }
        change_style(style);
        break;
    case UI_EVENT_SET_STROKE_STYLE:
        style.strokeStyle = (StrokeStyle)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_FONT:
        style.face = (TextFace)event->index;
        change_style(style);
        break;
    case UI_EVENT_SET_FONT_SIZE:
        style.fontSize = FONT_SIZES[event->index];
        change_style(style);
        break;
    case UI_EVENT_SET_LASER_MODE:
        controller_cancel(&app.controller);
        app.controller.laserPointer = event->index != 0;
        app.config.laserPointer = app.controller.laserPointer;
        save_config();
        app_flush();
        app_sync_ui();
        break;
    case UI_EVENT_SET_OPACITY:
        style.opacity = event->value;
        preview_style(style);
        break;
    case UI_EVENT_SLIDER_END:
        finish_style_drag();
        break;
    case UI_EVENT_PANEL_MOVED:
        app.config.ui.placement[event->panel].set = true;
        app.config.ui.placement[event->panel].x = event->x;
        app.config.ui.placement[event->panel].y = event->y;
        save_config();
        break;
    case UI_EVENT_PANEL_PINNED:
        app.config.ui.pinned[event->panel] = event->index != 0;
        save_config();
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
    if (controller_take_tool_finished(&app.controller) && !app.controller.toolLocked) {
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
static void handle_pointer(int phase, POINT screen, float pressure, bool contact);

/**
 * @brief Decides what to do with input from a pointer other than the one that is drawing.
 *
 * A pointer that is no longer in contact has lost its release; it is ended so that a new press can
 * start. Otherwise the foreign input is to be ignored.
 * @param isPress true when the foreign input is a new press.
 * @return true when the active pointer was stale and has been ended, so the new input may proceed.
 */
static bool supersede_stale_pointer(bool isPress)
{
    if (!isPress) {
        return false;
    }
    bool alive;
    if (app.activePointer == 0) {
        alive = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    } else {
        POINTER_INFO info;
        alive = GetPointerInfo(app.activePointer, &info) && (info.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
    }
    if (alive) {
        return false;
    }
    LOG("stale pointer %u ended before a new press", (unsigned)app.activePointer);
    handle_pointer(2, (POINT){0, 0}, -1.0f, false);
    return true;
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
        bool found = false;
        if (key >= '0' && key <= '9') {
            found = tool_at_slot(app.config.toolOrder, app.config.hiddenTools,
                                 key == '0' ? 9 : (int)key - '1', &tool);
        } else {
            found = tool_from_shortcut((int)key, &tool);
        }
        if (found) {
            select_tool(tool);
        }
    }
}

/**
 * @brief Overlay window procedure; routes pointer, keyboard and cursor messages.
 * @param window Overlay window handle.
 * @param message Window message.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return Message result.
 */
static LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        UINT32 id = GET_POINTERID_WPARAM(wparam);
        if (app.pointerDown && id != app.activePointer &&
            !supersede_stale_pointer(message == WM_POINTERDOWN)) {
            return 0;
        }
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
        if (phase == 0) {
            app.activePointer = id;
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
        if (app.pointerDown && !supersede_stale_pointer(true)) {
            return 0;
        }
        app.activePointer = 0;
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
    LOG("display change: %d monitor(s)", app.monitorCount);
    if (app.ui) {
        ui_place_defaults(app.ui, app.monitors[panel_monitor()].work, false);
    }
    if (app.overlayReady) {
        controller_cancel(&app.controller);
        app_apply_state();
    }
}

/**
 * @brief Recovers after sleep or a locked session: resets input hooks, re-presents windows, restores the tray
 * icon.
 *
 * Input that happened while the machine slept or the session was locked is lost, and the system may
 * have dropped the low-level hooks, so held buttons are released and the hooks are installed again.
 */
static void handle_resume(void)
{
    LOG("resume or unlock: hooks reset, pointer down=%d", app.pointerDown);
    hotkeys_reset_hooks(app.hotkeys);
    if (app.pointerDown) {
        handle_pointer(2, (POINT){0, 0}, -1.0f, false);
    }
    if (app.overlayReady) {
        overlay_raise(&app.overlay);
        render_everything();
    }
    if (app.ui) {
        ui_raise(app.ui);
    }
    if (!tray_is_registered(app.tray)) {
        restore_tray(false);
    }
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
    state->drawMonitor = draw_monitor();
    state->uiMonitor = panel_monitor();
    state->hideUiFromCapture = app.config.ui.excludeFromCapture;
    state->autoHideUi = app.config.ui.autoHide;
    state->startWithWindows = autostart_get();
    state->learning = app.learning;
}

/**
 * @brief Applies the loaded configuration to the controller, the interface and the overlay.
 */
static void apply_config(void)
{
    refresh_monitors();
    app.controller.style.stroke = STROKE_PALETTE[app.config.defaultStrokeIndex];
    app.controller.style.fill = FILL_PALETTE[app.config.defaultFillIndex];
    app.controller.style.width = WIDTH_PRESETS[app.config.defaultWidthIndex];
    app.controller.toolLocked = app.config.toolLock;
    app.controller.laserPointer = app.config.laserPointer;
    log_set(app.config.debugLog, app.logPath);
    if (app.ui) {
        ui_apply_settings(app.ui, &app.config.ui);
    }
    if (app.overlayReady) {
        controller_cancel(&app.controller);
        app_apply_state();
    }
}

/**
 * @brief Loads the configuration file and applies it everywhere.
 */
static void reload_config(void)
{
    wchar_t errors[256] = L"";
    ConfigStatus status = config_load(&app.config, app.configPath, errors, 256);
    app.configProtected = status == CONFIG_UNREADABLE;
    log_set(app.config.debugLog, app.logPath);
    LOG("config %s: %ls",
        status == CONFIG_LOADED ? "loaded" : (status == CONFIG_MISSING ? "missing, defaults" : "unreadable"),
        errors[0] ? errors : L"no warnings");
    if (status == CONFIG_MISSING) {
        save_config();
    } else if (status == CONFIG_UNREADABLE) {
        notify(L"Configuration not read", L"The settings file could not be read. It was left unchanged and "
                                          L"the current settings stay in use.");
    }
    if (errors[0]) {
        notify(L"Configuration warnings", errors);
    }
    apply_hotkeys();
    apply_config();
}

/**
 * @brief Puts every option back to its default; button bindings, custom colors and panel positions stay.
 */
static void reset_options(void)
{
    Config fresh;
    config_defaults(&fresh);
    memcpy(fresh.bindings, app.config.bindings, sizeof fresh.bindings);
    fresh.bindingCount = app.config.bindingCount;
    memcpy(fresh.customColors, app.config.customColors, sizeof fresh.customColors);
    fresh.customCount = app.config.customCount;
    memcpy(fresh.ui.placement, app.config.ui.placement, sizeof fresh.ui.placement);
    app.config = fresh;
    controller_set_tool(&app.controller, app.config.defaultTool);
    save_config();
    apply_config();
    app_sync_ui();
}

/**
 * @brief Puts the toolbar and the panels back at their default places on the tools screen.
 */
static void reset_positions(void)
{
    for (int i = 0; i < PANEL_COUNT; i++) {
        app.config.ui.placement[i].set = false;
    }
    save_config();
    if (app.ui) {
        ui_place_defaults(app.ui, app.monitors[panel_monitor()].work, true);
    }
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
        save_config();
        controller_cancel(&app.controller);
        app_apply_state();
        break;
    case TRAY_SET_UI_MONITOR:
        app.config.uiMonitor = event->value;
        for (int i = 0; i < PANEL_COUNT; i++) {
            app.config.ui.placement[i].set = false;
        }
        save_config();
        if (app.ui) {
            ui_place_defaults(app.ui, app.monitors[panel_monitor()].work, true);
        }
        break;
    case TRAY_TOGGLE_HIDE_UI_FROM_CAPTURE:
        app.config.ui.excludeFromCapture = !app.config.ui.excludeFromCapture;
        if (app.ui) {
            ui_set_capture_excluded(app.ui, app.config.ui.excludeFromCapture);
        }
        save_config();
        break;
    case TRAY_TOGGLE_AUTO_HIDE_UI:
        app.config.ui.autoHide = !app.config.ui.autoHide;
        save_config();
        if (app.ui) {
            ui_apply_settings(app.ui, &app.config.ui);
        }
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
        if (app.configPath[0]) {
            ShellExecuteW(NULL, L"open", app.configPath, NULL, NULL, SW_SHOWNORMAL);
        }
        break;
    case TRAY_COPY_PNG:
        copy_drawing_png();
        break;
    case TRAY_SAVE_PNG:
        save_drawing_png();
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
                animationTimerOn = false;
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
        if (wparam == TIMER_RESUME_HOTKEYS) {
            KillTimer(window, TIMER_RESUME_HOTKEYS);
            apply_hotkeys();
            return 0;
        }
        if (wparam == TIMER_TRAY_RETRY) {
            KillTimer(window, TIMER_TRAY_RETRY);
            restore_tray(false);
            return 0;
        }
        break;
    case WM_APP_TRAY: {
        TrayState state;
        TrayEvent event;
        build_tray_state(&state);
        if (app.tray && tray_handle_message(app.tray, lparam, &state, &event)) {
            handle_tray_event(&event);
        }
        return 0;
    }
    case WM_APP_ACTIVATE:
        set_drawing(!app.drawing);
        return 0;
    case WM_CLOSE:
        app.quitting = true;
        PostQuitMessage(0);
        return 0;
    case WM_DISPLAYCHANGE:
        handle_display_change();
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wparam == WTS_SESSION_UNLOCK) {
            handle_resume();
        } else if (wparam == WTS_SESSION_LOCK) {
            hotkeys_reset_hooks(app.hotkeys);
        }
        return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) {
            handle_display_change();
            handle_resume();
        }
        return TRUE;
    case WM_DESTROY:
        return 0;
    default:
        if (app.taskbarCreatedMessage != 0 && message == app.taskbarCreatedMessage) {
            restore_tray(true);
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
 * @param info Exception information; logged when the log is on.
 * @return EXCEPTION_EXECUTE_HANDLER to terminate after cleanup.
 */
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *info)
{
    LOG("CRASH code=0x%08lX address=%p", info->ExceptionRecord->ExceptionCode,
        info->ExceptionRecord->ExceptionAddress);
    if (app.overlayReady && app.overlay.window) {
        overlay_set_visible(&app.overlay, false);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

/**
 * @brief Builds the path of a file next to the executable, never writing past the buffer.
 * @param name File name to append, including its terminator.
 * @param nameChars Length of the name including the terminator, in characters.
 * @param path Output buffer.
 * @param size Capacity of the buffer in characters.
 * @return true on success; on false the path is empty because the executable path does not fit.
 */
static bool build_sibling_path(const wchar_t *name, size_t nameChars, wchar_t *path, size_t size)
{
    path[0] = L'\0';
    DWORD length = GetModuleFileNameW(NULL, path, (DWORD)size);
    if (length == 0 || length >= size) {
        path[0] = L'\0';
        return false;
    }
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash || (size_t)(path + size - (slash + 1)) < nameChars) {
        path[0] = L'\0';
        return false;
    }
    memcpy(slash + 1, name, nameChars * sizeof(wchar_t));
    return true;
}

/**
 * @brief Builds the configuration file path next to the executable and adopts a legacy file once.
 *
 * When the current file does not exist and a file of the former product name sits next to it,
 * that file is copied to the current name before the configuration is loaded (note 0092).
 * @param path Output buffer.
 * @param size Capacity of the buffer in characters.
 * @return true on success; on false the path is empty because the executable path does not fit.
 */
static bool build_config_path(wchar_t *path, size_t size)
{
    static const wchar_t FILE_NAME[] = L"PenPane.ini";
    static const wchar_t LEGACY_FILE_NAME[] = L"ink.ini";
    if (!build_sibling_path(FILE_NAME, ARRAYSIZE(FILE_NAME), path, size)) {
        return false;
    }
    wchar_t legacy[MAX_PATH];
    if (build_sibling_path(LEGACY_FILE_NAME, ARRAYSIZE(LEGACY_FILE_NAME), legacy, ARRAYSIZE(legacy))) {
        config_migrate_legacy(legacy, path);
    }
    return true;
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
 * @brief Creates the hidden window that owns timers, hotkeys, the tray icon and system notifications.
 *
 * It is a top-level popup that is never shown, not a message-only window, because message-only
 * windows do not receive broadcasts such as "TaskbarCreated", WM_DISPLAYCHANGE and power events.
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
    app.messageWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, MESSAGE_CLASS, L"PenPane",
                                        WS_POPUP, 0, 0, 0, 0, NULL, NULL, app.instance, NULL);
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
    bool pathFits = build_config_path(app.configPath, ARRAYSIZE(app.configPath));

    app.hotkeys = hotkeys_create(app.messageWindow, on_hotkey_action, on_learn_result, NULL);
    register_emergency_hotkey();

    if (!pathFits) {
        notify(L"Settings location unavailable",
               L"The folder of PenPane is too deeply nested to keep a settings file there. Defaults are used "
               L"and nothing is saved.");
    }
    static const wchar_t LOG_NAME[] = L"PenPane.log";
    build_sibling_path(LOG_NAME, ARRAYSIZE(LOG_NAME), app.logPath, ARRAYSIZE(app.logPath));
    reload_config();
    LOG("started PenPane %s, %d monitor(s)", PENPANE_VERSION_STRING, app.monitorCount);
    controller_set_tool(&app.controller, app.config.defaultTool);
    app.ui = ui_create(instance, on_ui_event, NULL, &app.config.ui);
    if (app.ui) {
        ui_set_capture_excluded(app.ui, app.config.ui.excludeFromCapture);
        ui_place_defaults(app.ui, app.monitors[panel_monitor()].work, false);
        ui_set_visible(app.ui, true, false);
        app_sync_ui();
    } else {
        notify(L"PenPane", L"The toolbar could not be created. Use the tray menu instead.");
    }
    create_tray();
    WTSRegisterSessionNotification(app.messageWindow, NOTIFY_FOR_THIS_SESSION);
    app.powerNotification = RegisterSuspendResumeNotification(app.messageWindow, DEVICE_NOTIFY_WINDOW_HANDLE);

    trim_working_set();

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (app.powerNotification) {
        UnregisterSuspendResumeNotification(app.powerNotification);
    }
    WTSUnRegisterSessionNotification(app.messageWindow);
    if (app.emergencyRegistered) {
        UnregisterHotKey(app.messageWindow, EMERGENCY_HOTKEY_ID);
    }
    destroy_overlay();
    ui_destroy(app.ui);
    hotkeys_destroy(app.hotkeys);
    tray_destroy(app.tray);
    controller_destroy(&app.controller);
    scene_destroy(&app.scenes[0]);
    scene_destroy(&app.scenes[1]);
    text_shutdown();
    jobs_shutdown(JOBS_SHUTDOWN_WAIT_MS);
    DestroyWindow(app.messageWindow);
    return 0;
}
