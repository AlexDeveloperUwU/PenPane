/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file ui.h
 * @brief Floating toolbar, properties panel and history panel in Excalidraw dark style.
 *
 * The UI consists of three independent small layered windows (islands) that can be dragged to any
 * monitor and excluded from screen capture. The module owns drawing and hit testing only; every
 * user choice is reported through the event handler and the application pushes state back with
 * ui_update.
 */
#ifndef PENPANE_UI_H
#define PENPANE_UI_H

#include <stdbool.h>
#include <windows.h>

#include "actions.h"
#include "style.h"
#include "tool.h"

/**
 * @brief The floating islands.
 *
 * PANEL_COUNT counts the panels with saved placement and pin state. The dialogs (settings, clear
 * confirmation, color picker) are extra islands that are centered when they open, never persisted
 * and open or close independently of each other.
 */
typedef enum PanelId {
    PANEL_TOOLBAR,
    PANEL_PROPERTIES,
    PANEL_HISTORY,
    PANEL_COUNT,
    PANEL_SETTINGS = PANEL_COUNT,
    PANEL_CONFIRM,
    PANEL_COLOR,
    PANEL_ISLAND_COUNT
} PanelId;

/** @brief Tells whether an island is a dialog. */
#define UI_PANEL_IS_DIALOG(panel) ((panel) >= PANEL_COUNT)
/** @brief Bit of a dialog island in UiModel.dialogs. */
#define UI_DIALOG_BIT(panel) (1u << (panel))

/** @brief Persisted top-left screen position of one island. */
typedef struct PanelPlacement {
    bool set;
    int x;
    int y;
} PanelPlacement;

/** @brief UI preferences loaded from the configuration file. */
typedef struct UiSettings {
    PanelPlacement placement[PANEL_ISLAND_COUNT];
    bool autoHide;
    int autoHideMs;
    bool excludeFromCapture;
    bool pinned[PANEL_ISLAND_COUNT];
} UiSettings;

/** @brief Sections shown in the properties panel; bitmask for UiModel.sections. */
#define UI_SECTION_STROKE 0x01u
#define UI_SECTION_FILL 0x02u
#define UI_SECTION_WIDTH 0x04u
#define UI_SECTION_STROKE_STYLE 0x08u
/** @brief Adds the variable (pressure) option to the width row. */
#define UI_SECTION_PRESSURE 0x10u
#define UI_SECTION_OPACITY 0x20u
#define UI_SECTION_FONT 0x40u
#define UI_SECTION_LASER 0x80u
#define UI_SECTION_ARROWHEADS 0x100u
#define UI_SECTION_FONT_SIZE 0x200u

/** @brief Characters of the text that names a tool's extra shortcut, for example "Ctrl+F15 x2". */
#define UI_SHORTCUT_CHARS 32

/**
 * @brief Application state the UI renders.
 *
 * toolShortcut holds the extra shortcut of each tool, empty when it has none. learningTool is the
 * tool whose shortcut is waiting for a button press, or -1. settingsTab is the tab of the settings window.
 */
typedef struct UiModel {
    Tool tool;
    bool toolLocked;
    Style style;
    unsigned sections;
    bool canUndo;
    bool canRedo;
    bool whiteboard;
    bool drawing;
    bool laserPointer;
    Tool toolOrder[TOOL_COUNT];
    unsigned hiddenTools;
    bool drawingsHidden;
    bool autoHide;
    bool excludeCapture;
    unsigned dialogs;
    bool pastel;
    bool debugLog;
    bool exportBackground;
    bool exportFullScreen;
    wchar_t toolShortcut[TOOL_COUNT][UI_SHORTCUT_CHARS];
    int learningTool;
    int settingsTab;
    Color custom[CUSTOM_COLOR_MAX];
    int customCount;
    float pickerHue;
    float pickerSat;
    float pickerVal;
} UiModel;

/** @brief Kinds of events the UI reports. */
typedef enum UiEventKind {
    UI_EVENT_ACTION,
    UI_EVENT_SELECT_TOOL,
    UI_EVENT_SET_STROKE,
    UI_EVENT_SET_FILL,
    UI_EVENT_SET_WIDTH,
    UI_EVENT_SET_STROKE_STYLE,
    UI_EVENT_SET_OPACITY,
    UI_EVENT_SET_FONT,
    UI_EVENT_SET_LASER_MODE,
    UI_EVENT_TOOL_MOVE,
    UI_EVENT_TOOL_VISIBLE,
    UI_EVENT_SET_AUTO_HIDE,
    UI_EVENT_SET_EXCLUDE_CAPTURE,
    UI_EVENT_CLOSE_DIALOG,
    UI_EVENT_SET_STROKE_COLOR,
    UI_EVENT_SET_PASTEL,
    UI_EVENT_SET_DEBUG_LOG,
    UI_EVENT_OPEN_PICKER,
    UI_EVENT_PICK_HUE,
    UI_EVENT_PICK_SV,
    UI_EVENT_EDIT_HEX,
    UI_EVENT_EDIT_END,
    UI_EVENT_SET_ARROWHEADS,
    UI_EVENT_RESET_OPTIONS,
    UI_EVENT_RESET_POSITIONS,
    UI_EVENT_SEND_KEY,
    UI_EVENT_TOOL_SHORTCUT,
    UI_EVENT_TOOL_SHORTCUT_CLEAR,
    UI_EVENT_TOOL_SHORTCUT_KEY,
    UI_EVENT_SETTINGS_TAB,
    UI_EVENT_PANEL_MOVED,
    UI_EVENT_SLIDER_END,
    UI_EVENT_PANEL_PINNED,
    UI_EVENT_SET_EXPORT_BACKGROUND,
    UI_EVENT_SET_EXPORT_FULL_SCREEN,
    UI_EVENT_SET_FONT_SIZE
} UiEventKind;

/**
 * @brief One user choice reported by the UI.
 *
 * Field use per kind:
 *  - UI_EVENT_ACTION: action holds the requested action (toggle_overlay, toggle_mode, undo, redo,
 *    toggle_tool_lock, emergency_exit).
 *  - UI_EVENT_SELECT_TOOL: tool holds the chosen tool.
 *  - UI_EVENT_SET_STROKE / SET_FILL / SET_WIDTH: index is the preset index in the matching table
 *    of style.h; for SET_WIDTH an index equal to WIDTH_PRESET_COUNT selects the variable (pressure) width.
 *  - UI_EVENT_SET_STROKE_STYLE: index is a StrokeStyle value.
 *  - UI_EVENT_SET_FONT: index is a TextFace value.
 *  - UI_EVENT_SET_FONT_SIZE: index is the preset index in FONT_SIZES of style.h.
 *  - UI_EVENT_SET_LASER_MODE: index is 0 for the fading trail and 1 for the pointer dot.
 *  - UI_EVENT_TOOL_MOVE: tool is moved one place in the toolbar; index is -1 (earlier) or 1 (later).
 *  - UI_EVENT_TOOL_VISIBLE: tool is shown (index 1) or hidden (index 0) in the toolbar.
 *  - UI_EVENT_SET_AUTO_HIDE / SET_EXCLUDE_CAPTURE: index is 0 or 1.
 *  - UI_EVENT_CLOSE_DIALOG: the dialog island in panel was dismissed.
 *  - UI_EVENT_SET_STROKE_COLOR: color is the chosen stroke color (pastel or custom swatch).
 *  - UI_EVENT_SET_PASTEL / SET_DEBUG_LOG / SET_EXPORT_BACKGROUND / SET_EXPORT_FULL_SCREEN: index is 0 or 1.
 *  - UI_EVENT_OPEN_PICKER: the color picker button was pressed.
 *  - UI_EVENT_PICK_HUE: value is the hue, 0 to 1. Sent while dragging, followed by UI_EVENT_SLIDER_END.
 *  - UI_EVENT_PICK_SV: value is the saturation and extra the brightness, both 0 to 1. Same drag rules.
 *  - UI_EVENT_EDIT_HEX: handled inside the UI (starts typing a hex color); never reaches the handler.
 *  - UI_EVENT_EDIT_END: typing a hex color ended; the application may take the keyboard focus back.
 *    A typed color is reported first as UI_EVENT_SET_STROKE_COLOR.
 *  - UI_EVENT_SET_ARROWHEADS: index is an ArrowHeads value.
 *  - UI_EVENT_RESET_OPTIONS: reset every option to its default.
 *  - UI_EVENT_RESET_POSITIONS: put the toolbar and panels back at their default places.
 *  - UI_EVENT_SEND_KEY: index is a virtual-key code to type into the window that has the keyboard focus.
 *  - UI_EVENT_TOOL_SHORTCUT: tool is the tool whose extra shortcut should be learned (a second click cancels).
 *  - UI_EVENT_TOOL_SHORTCUT_CLEAR: tool is the tool whose extra shortcut should be removed.
 *  - UI_EVENT_TOOL_SHORTCUT_KEY: tool is the tool being learned and index a virtual-key code (F13 to F18) to bind to it as a single press, without pressing it.
 *  - UI_EVENT_SETTINGS_TAB: index is the settings tab to show (0 General, 1 Toolbar, 2 Shortcuts, 3 Pen keys).
 *  - UI_EVENT_SET_OPACITY: value is 0.0 to 1.0 in steps of 0.1.
 *  - UI_EVENT_PANEL_MOVED: panel, x and y hold the new top-left position.
 *  - UI_EVENT_SLIDER_END: the slider drag that produced the preceding UI_EVENT_SET_OPACITY events is over
 *    (button released or capture lost); no other field is used.
 *  - UI_EVENT_PANEL_PINNED: panel is the island and index is 1 when it is now pinned open, 0 when it
 *    is unpinned. The UI has already applied the change to its own settings copy.
 */
typedef struct UiEvent {
    UiEventKind kind;
    Action action;
    Tool tool;
    int index;
    float value;
    float extra;
    Color color;
    PanelId panel;
    int x;
    int y;
} UiEvent;

/**
 * @brief Receives user choices.
 * @param user Opaque pointer given to ui_create.
 * @param event The choice.
 */
typedef void (*UiEventHandler)(void *user, const UiEvent *event);

/** @brief Opaque UI instance. */
typedef struct Ui Ui;

/**
 * @brief Creates the three island windows, hidden.
 * @param instance Module instance used to register window classes.
 * @param handler Receives user choices; called from the UI thread inside window messages.
 * @param user Opaque pointer forwarded to the handler.
 * @param settings Initial placements and preferences; copied.
 * @return New instance, or NULL on failure.
 */
Ui *ui_create(HINSTANCE instance, UiEventHandler handler, void *user, const UiSettings *settings);

/**
 * @brief Destroys all windows and frees the instance.
 * @param ui Instance to destroy, may be NULL.
 */
void ui_destroy(Ui *ui);

/**
 * @brief Pushes new application state and repaints only what changed.
 * @param ui Instance to update.
 * @param model Current application state; copied.
 */
void ui_update(Ui *ui, const UiModel *model);

/**
 * @brief Applies changed preferences at runtime: auto-hide, its delay, pinned islands and capture exclusion.
 *
 * Pinned islands (and every island when auto-hide is off) expand at once; unpinned expanded islands
 * collapse after the delay once the pointer is away. Saved placements are replaced too but islands
 * are not moved; use ui_place_defaults for that.
 * @param ui Instance to change.
 * @param settings New preferences; copied.
 */
void ui_apply_settings(Ui *ui, const UiSettings *settings);

/**
 * @brief Shows or hides the UI for the current overlay state.
 *
 * With drawing true all islands are shown. With drawing false only the toolbar stays visible, so the
 * user can open it and start drawing from its button. An island that is not pinned shows as a handle
 * until it is clicked and collapses again shortly after the pointer leaves; a pinned island stays
 * open. With visible false everything is hidden.
 * @param ui Instance to change.
 * @param visible false hides every island.
 * @param drawing true while the overlay captures input.
 */
void ui_set_visible(Ui *ui, bool visible, bool drawing);

/**
 * @brief Places islands that have no saved position at their defaults on a monitor.
 *
 * Defaults mirror Excalidraw: toolbar centered at the top, properties panel at the left,
 * history panel at the bottom left.
 * @param ui Instance to change.
 * @param workArea Work area of the monitor that should host the UI.
 * @param forceAll true to ignore saved positions and reposition every island. In that case every
 *        new position is also reported with UI_EVENT_PANEL_MOVED so that it can be persisted.
 */
void ui_place_defaults(Ui *ui, RECT workArea, bool forceAll);

/**
 * @brief Excludes or includes the islands in screen capture (WDA_EXCLUDEFROMCAPTURE).
 * @param ui Instance to change.
 * @param exclude true to hide the islands from capture.
 * @return false when the OS does not support exclusion.
 */
bool ui_set_capture_excluded(Ui *ui, bool exclude);

/**
 * @brief Raises all visible islands above the overlay without activating them.
 * @param ui Instance to change.
 */
void ui_raise(Ui *ui);

/**
 * @brief Tells whether a screen point is over a visible island.
 * @param ui Instance to query.
 * @param point Screen coordinates.
 * @return true when the point is inside any visible island window.
 */
bool ui_contains_point(const Ui *ui, POINT point);

#ifdef PENPANE_UI_DUMP
/**
 * @brief Test hook: writes every shown island, composited at its screen position over a plain backdrop, as a BMP.
 * @param ui Instance to dump.
 * @param path Destination file, narrow characters.
 * @return true on success.
 */
bool ui_dump(const Ui *ui, const char *path);

/**
 * @brief Test hook: presses, drags and releases on the first item of a kind, as the mouse would.
 * @param ui Instance to drive.
 * @param panel Island holding the item.
 * @param kind ItemKind value of the item.
 * @param points Positions as fractions of the item rectangle; the first is the press, the last the release.
 * @param count Number of positions, at least two.
 * @return false when the island or the item does not exist.
 */
bool ui_test_drag(Ui *ui, PanelId panel, int kind, const float (*points)[2], int count);
#endif

#endif
