/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file controller.h
 * @brief Turns pointer and keyboard input into tool behavior: drawing, selecting, erasing, laser, text.
 *
 * The controller owns transient interaction state (draft element, selection, trails) and mutates
 * the attached scene. It never touches the screen; it reports which regions need repaint and
 * which were already drawn directly, and draws its own overlays on request.
 */
#ifndef PENPANE_CONTROLLER_H
#define PENPANE_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "pen.h"
#include "render.h"
#include "scene.h"
#include "tool.h"
#include "trail.h"

/** @brief One pointer event. */
typedef struct PointerSample {
    Point position;
    float pressure;
    uint32_t timeMs;
    bool shift;
} PointerSample;

/** @brief Kinds of in-progress pointer interaction. */
typedef enum Interaction {
    INTERACTION_NONE,
    INTERACTION_DRAW,
    INTERACTION_MOVE,
    INTERACTION_RESIZE,
    INTERACTION_ERASE,
    INTERACTION_LASER
} Interaction;

/**
 * @brief Tool controller state.
 *
 * While editingText is set, typed characters go to the draft element, or, when editIndex is not negative, to
 * the label of the scene element at that index (textBefore holds its copy from before the edit).
 * lastDownMs, lastDownPosition and hasLastDown remember the previous pointer press for double click
 * detection.
 */
typedef struct Controller {
    Scene *scene;
    Canvas *canvas;
    Tool tool;
    Style style;
    bool toolLocked;
    Interaction interaction;
    Element draft;
    bool hasDraft;
    PenBuilder pen;
    int penGroup;
    RECT draftBounds;
    int selected;
    Element before;
    bool hasBefore;
    Element styleBefore;
    bool hasStyleBefore;
    Point dragStart;
    int handle;
    FloatRect startBox;
    Trail laser;
    Trail eraser;
    bool laserPointer;
    bool laserDotVisible;
    Point laserDot;
    bool editingText;
    int editIndex;
    Element textBefore;
    bool hasTextBefore;
    uint32_t lastDownMs;
    Point lastDownPosition;
    bool hasLastDown;
    Dirty repaint;
    Dirty direct;
    bool toolFinished;
} Controller;

/**
 * @brief Initializes a controller with the default style and the pen tool.
 * @param controller Controller to initialize.
 */
void controller_init(Controller *controller);

/**
 * @brief Releases transient state.
 * @param controller Controller to release.
 */
void controller_destroy(Controller *controller);

/**
 * @brief Attaches the scene and canvas that subsequent input affects, cancelling any interaction.
 * @param controller Controller to configure.
 * @param scene Active scene.
 * @param canvas Canvas used for direct pen drawing.
 */
void controller_attach(Controller *controller, Scene *scene, Canvas *canvas);

/**
 * @brief Cancels any interaction in progress, committing text being edited.
 * @param controller Controller to reset.
 */
void controller_cancel(Controller *controller);

/**
 * @brief Selects the active tool; commits text and clears the selection when appropriate.
 * @param controller Controller to configure.
 * @param tool New tool.
 */
void controller_set_tool(Controller *controller, Tool tool);

/**
 * @brief Drops the selection and schedules a repaint of its decoration.
 * @param controller Controller to update.
 */
void controller_deselect(Controller *controller);

/**
 * @brief Handles pointer press.
 * @param controller Controller to drive.
 * @param sample Pointer data.
 */
void controller_pointer_down(Controller *controller, const PointerSample *sample);

/**
 * @brief Handles pointer motion, with or without contact.
 * @param controller Controller to drive.
 * @param sample Pointer data.
 * @param down true while the pointer is in contact.
 */
void controller_pointer_move(Controller *controller, const PointerSample *sample, bool down);

/**
 * @brief Handles pointer release.
 * @param controller Controller to drive.
 * @param sample Pointer data.
 */
void controller_pointer_up(Controller *controller, const PointerSample *sample);

/**
 * @brief Deletes the selected element as one undoable step.
 * @param controller Controller to drive.
 */
void controller_delete_selection(Controller *controller);

/**
 * @brief Replaces the style of the selected element, recording one undoable step.
 * @param controller Controller to drive.
 * @param style New style.
 */
void controller_apply_style_to_selection(Controller *controller, const Style *style);

/**
 * @brief Applies a style to the selected element without recording history, remembering its original state
 * once.
 *
 * Meant for continuous edits such as dragging a slider. Finish with controller_style_commit so the
 * whole edit becomes a single undo step.
 * @param controller Controller to drive.
 * @param style New style.
 */
void controller_style_preview(Controller *controller, const Style *style);

/**
 * @brief Records the edits made by controller_style_preview as one undoable step; does nothing when none are
 * pending.
 * @param controller Controller to drive.
 */
void controller_style_commit(Controller *controller);

/**
 * @brief Types a character into the text being edited.
 * @param controller Controller to drive.
 * @param character UTF-16 code unit; backspace (8) deletes, carriage return inserts a line break.
 * @return true when the character was consumed.
 */
bool controller_text_input(Controller *controller, wchar_t character);

/**
 * @brief Finishes text editing, adding the text to the scene if it is not empty.
 * @param controller Controller to drive.
 */
void controller_text_commit(Controller *controller);

/**
 * @brief Advances trail fading.
 * @param controller Controller to animate.
 * @param nowMs Monotonic millisecond clock.
 * @return true while trails are still visible and another frame is needed.
 */
bool controller_animate(Controller *controller, uint32_t nowMs);

/**
 * @brief Draws overlays that sit above the scene: draft, selection, caret and trails.
 * @param controller Controller whose overlays are drawn.
 * @param canvas Target canvas.
 * @param clip Pixel rectangle to restrict drawing to.
 * @param nowMs Monotonic millisecond clock.
 */
void controller_draw_overlays(const Controller *controller, Canvas *canvas, const RECT *clip, uint32_t nowMs);

/**
 * @brief Grows an extent by the bounds of transient drawing: draft, text caret, selection handles and trails.
 *
 * Margins are generous so that handles, caret and trail width stay inside the bounds.
 * @param controller Controller to query.
 * @param extent Dirty region to grow; unchanged when nothing transient is visible.
 */
void controller_add_transient_bounds(const Controller *controller, Dirty *extent);

/**
 * @brief Collects regions that changed.
 * @param controller Controller to query.
 * @param repaint Receives regions needing a clear-and-redraw, including scene changes.
 * @param direct Receives regions already drawn on the canvas that only need presenting.
 */
void controller_take_dirty(Controller *controller, Dirty *repaint, Dirty *direct);

/**
 * @brief Returns whether a stroke finished and the tool should revert (when tool lock is off).
 * @param controller Controller to query.
 * @return true once per finished stroke.
 */
bool controller_take_tool_finished(Controller *controller);

/**
 * @brief Returns the selected element, or NULL.
 * @param controller Controller to query.
 * @return Pointer into the scene, valid until the scene changes.
 */
const Element *controller_selection(const Controller *controller);

/**
 * @brief Tells whether any interaction needs more frames or events.
 * @param controller Controller to query.
 * @return true while a pointer interaction or text edit is active.
 */
bool controller_is_busy(const Controller *controller);

#endif
