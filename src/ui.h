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

/** @brief The three floating islands. */
typedef enum PanelId {
    PANEL_TOOLBAR,
    PANEL_PROPERTIES,
    PANEL_HISTORY,
    PANEL_COUNT
} PanelId;

/** @brief Persisted top-left screen position of one island. */
typedef struct PanelPlacement {
    bool set;
    int x;
    int y;
} PanelPlacement;

/** @brief UI preferences loaded from the configuration file. */
typedef struct UiSettings {
    PanelPlacement placement[PANEL_COUNT];
    bool autoHide;
    int autoHideMs;
    bool excludeFromCapture;
    bool pinned[PANEL_COUNT];
} UiSettings;

/** @brief Sections shown in the properties panel; bitmask for UiModel.sections. */
#define UI_SECTION_STROKE 0x01u
#define UI_SECTION_FILL 0x02u
#define UI_SECTION_WIDTH 0x04u
#define UI_SECTION_STROKE_STYLE 0x08u
#define UI_SECTION_PRESSURE 0x10u
#define UI_SECTION_OPACITY 0x20u

/** @brief Application state the UI renders. */
typedef struct UiModel {
    Tool tool;
    bool toolLocked;
    Style style;
    unsigned sections;
    bool canUndo;
    bool canRedo;
    bool whiteboard;
    bool drawing;
} UiModel;

/** @brief Kinds of events the UI reports. */
typedef enum UiEventKind {
    UI_EVENT_ACTION,
    UI_EVENT_SELECT_TOOL,
    UI_EVENT_SET_STROKE,
    UI_EVENT_SET_FILL,
    UI_EVENT_SET_WIDTH,
    UI_EVENT_SET_STROKE_STYLE,
    UI_EVENT_SET_PRESSURE,
    UI_EVENT_SET_OPACITY,
    UI_EVENT_PANEL_MOVED,
    UI_EVENT_SLIDER_END,
    UI_EVENT_PANEL_PINNED
} UiEventKind;

/**
 * @brief One user choice reported by the UI.
 *
 * Field use per kind:
 *  - UI_EVENT_ACTION: action holds the requested action (toggle_overlay, toggle_mode, undo, redo,
 *    toggle_tool_lock, emergency_exit).
 *  - UI_EVENT_SELECT_TOOL: tool holds the chosen tool.
 *  - UI_EVENT_SET_STROKE / SET_FILL / SET_WIDTH: index is the preset index in the matching table
 *    of style.h.
 *  - UI_EVENT_SET_STROKE_STYLE: index is a StrokeStyle value.
 *  - UI_EVENT_SET_PRESSURE: index is 0 or 1.
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

#endif
