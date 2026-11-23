/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "controller.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hittest.h"

#define SELECT_TOLERANCE 6.0f
#define ERASE_TOLERANCE 3.0f
#define ERASE_SAMPLE_STEP 4.0f
#define MIN_SHAPE_SIZE 3.0f
#define MIN_BOX_SIZE 6.0f
#define PI_FLOAT 3.14159265f
#define SNAP_ANGLE (PI_FLOAT / 12.0f)
#define LASER_RADIUS TRAIL_LASER_RADIUS
#define LASER_LIFETIME_MS TRAIL_LASER_LIFETIME_MS
#define ERASER_RADIUS TRAIL_ERASER_RADIUS
#define ERASER_LIFETIME_MS TRAIL_ERASER_LIFETIME_MS
#define ERASER_COLOR COLOR_ERASER_TRAIL
#define TRANSIENT_MARGIN 16

/**
 * @brief Adds a rectangle to the repaint region.
 * @param controller Controller to update.
 * @param rect Rectangle to add.
 */
static void repaint_add(Controller *controller, RECT rect)
{
    dirty_add(&controller->repaint, rect);
}

/**
 * @brief Maps a stroke width preset to a font size.
 * @param width Stroke width.
 * @return Font height in pixels.
 */
static int font_size_for_width(float width)
{
    if (width <= WIDTH_PRESETS[0] + 0.01f) {
        return 16;
    }
    if (width >= WIDTH_PRESETS[2] - 0.01f) {
        return 28;
    }
    return 20;
}

void controller_init(Controller *controller)
{
    memset(controller, 0, sizeof *controller);
    controller->tool = TOOL_PEN;
    controller->style = style_default();
    controller->selected = -1;
    controller->handle = HIT_HANDLE_NONE;
    trail_init(&controller->laser, LASER_RADIUS, LASER_LIFETIME_MS, COLOR_LASER, true);
    trail_init(&controller->eraser, ERASER_RADIUS, ERASER_LIFETIME_MS, ERASER_COLOR, false);
    dirty_reset(&controller->repaint);
    dirty_reset(&controller->direct);
}

void controller_destroy(Controller *controller)
{
    controller_cancel(controller);
}

/**
 * @brief Discards the draft element.
 * @param controller Controller to update.
 */
static void drop_draft(Controller *controller)
{
    if (controller->hasDraft) {
        repaint_add(controller, controller->draftBounds);
        element_free(&controller->draft);
        controller->hasDraft = false;
    }
}

/**
 * @brief Discards the saved pre-change copy.
 * @param controller Controller to update.
 */
static void drop_before(Controller *controller)
{
    if (controller->hasBefore) {
        element_free(&controller->before);
        controller->hasBefore = false;
    }
}

/**
 * @brief Clears erase markers on every element.
 * @param controller Controller whose scene is cleaned.
 */
static void unmark_erasing(Controller *controller)
{
    if (!controller->scene) {
        return;
    }
    for (int i = 0; i < controller->scene->count; i++) {
        Element *element = &controller->scene->items[i];
        if (element->erasing) {
            element->erasing = false;
            scene_mark_dirty(controller->scene, element_bounds(element));
        }
    }
}

/**
 * @brief Replaces an element in place with a copy of another, keeping the original when copying fails.
 * @param scene Scene owning the element, used to schedule the repaint.
 * @param item Element to overwrite.
 * @param source Element to copy from.
 * @return true when the copy succeeded and item now holds it.
 */
static bool replace_with_copy(Scene *scene, Element *item, const Element *source)
{
    Element copy;
    if (!element_copy(&copy, source)) {
        return false;
    }
    scene_mark_dirty(scene, element_bounds(item));
    element_free(item);
    *item = copy;
    scene_mark_dirty(scene, element_bounds(item));
    return true;
}

/**
 * @brief Drops the current selection and repaints its decoration.
 * @param controller Controller to update.
 */
static void clear_selection(Controller *controller)
{
    if (controller->selected >= 0 && controller->selected < controller->scene->count) {
        repaint_add(controller, render_selection_bounds(&controller->scene->items[controller->selected]));
    }
    controller->selected = -1;
}

void controller_cancel(Controller *controller)
{
    controller_style_commit(controller);
    if (controller->editingText) {
        controller_text_commit(controller);
    }
    if (controller->scene) {
        unmark_erasing(controller);
        if (controller->interaction == INTERACTION_MOVE || controller->interaction == INTERACTION_RESIZE) {
            if (controller->hasBefore && controller->selected >= 0 &&
                controller->selected < controller->scene->count) {
                Element *item = &controller->scene->items[controller->selected];
                replace_with_copy(controller->scene, item, &controller->before);
            }
        }
    }
    drop_draft(controller);
    drop_before(controller);
    if (controller->laser.count > 0) {
        repaint_add(controller, trail_bounds(&controller->laser));
        trail_clear(&controller->laser);
    }
    if (controller->eraser.count > 0) {
        repaint_add(controller, trail_bounds(&controller->eraser));
        trail_clear(&controller->eraser);
    }
    if (controller->laserDotVisible) {
        repaint_add(controller, laser_dot_bounds(controller->laserDot));
        controller->laserDotVisible = false;
    }
    controller->interaction = INTERACTION_NONE;
    controller->toolFinished = false;
}

void controller_attach(Controller *controller, Scene *scene, Canvas *canvas)
{
    controller_cancel(controller);
    if (controller->scene && controller->scene != scene) {
        controller->selected = -1;
    }
    controller->scene = scene;
    controller->canvas = canvas;
}

void controller_deselect(Controller *controller)
{
    if (controller->scene) {
        clear_selection(controller);
    }
    controller->selected = -1;
}

void controller_set_tool(Controller *controller, Tool tool)
{
    if (controller->tool == tool) {
        return;
    }
    controller_cancel(controller);
    if (controller->tool == TOOL_SELECT && controller->scene) {
        clear_selection(controller);
    }
    controller->tool = tool;
}

/**
 * @brief Applies shift constraints and updates the draft geometry for a shape tool.
 * @param controller Controller holding the draft.
 * @param position Current pointer position.
 * @param shift true when the shift key is held.
 */
static void update_shape_draft(Controller *controller, Point position, bool shift)
{
    Element *draft = &controller->draft;
    Point origin = draft->from;
    float dx = position.x - origin.x;
    float dy = position.y - origin.y;
    if (shift) {
        if (draft->type == ELEMENT_LINE || draft->type == ELEMENT_ARROW) {
            float length = sqrtf(dx * dx + dy * dy);
            float angle = roundf(atan2f(dy, dx) / SNAP_ANGLE) * SNAP_ANGLE;
            dx = cosf(angle) * length;
            dy = sinf(angle) * length;
        } else {
            float size = fmaxf(fabsf(dx), fabsf(dy));
            dx = dx < 0.0f ? -size : size;
            dy = dy < 0.0f ? -size : size;
        }
    }
    draft->to.x = origin.x + dx;
    draft->to.y = origin.y + dy;
}

/**
 * @brief Builds the draft element for the active drawing tool.
 * @param controller Controller to update.
 * @param sample Pointer data.
 * @return true when a draft was created.
 */
static bool begin_draft(Controller *controller, const PointerSample *sample)
{
    static const ElementType TYPES[TOOL_COUNT] = {ELEMENT_PEN,   ELEMENT_RECTANGLE, ELEMENT_ELLIPSE,
                                                  ELEMENT_ARROW, ELEMENT_LINE,      ELEMENT_PEN,
                                                  ELEMENT_PEN,   ELEMENT_TEXT,      ELEMENT_PEN,
                                                  ELEMENT_DIAMOND};
    controller->draft = element_make(TYPES[controller->tool], &controller->style);
    controller->hasDraft = true;
    controller->draft.from = sample->position;
    controller->draft.to = sample->position;
    controller->penGroup = 0;
    if (controller->draft.type == ELEMENT_PEN) {
        float pressure = controller->style.pressure && sample->pressure >= 0.0f ? sample->pressure : PEN_PRESSURE_CONSTANT;
        if (!pen_begin(&controller->pen, &controller->draft, sample->position, pressure)) {
            element_free(&controller->draft);
            controller->hasDraft = false;
            return false;
        }
    }
    controller->draftBounds = element_bounds(&controller->draft);
    return true;
}

/**
 * @brief Draws the newest pen segments directly or schedules a repaint.
 * @param controller Controller holding the pen draft.
 */
static void flush_pen_draft(Controller *controller)
{
    Element *draft = &controller->draft;
    RECT everything = canvas_area(controller->canvas);
    bool opaque = draft->style.opacity >= 0.999f && (draft->style.stroke >> 24) == 0xFF;
    if (opaque) {
        int firstGroup = controller->penGroup > 0 ? controller->penGroup - 1 : 0;
        RECT touched = render_pen_tail(controller->canvas, draft, firstGroup, &everything);
        dirty_add(&controller->direct, touched);
        controller->penGroup = draft->pointCount - 1;
        controller->draftBounds = element_bounds(draft);
    } else {
        repaint_add(controller, controller->draftBounds);
        controller->draftBounds = element_bounds(draft);
        repaint_add(controller, controller->draftBounds);
    }
}

/**
 * @brief Marks erasable elements touched along a trail segment.
 * @param controller Controller to update.
 * @param from Segment start.
 * @param to Segment end.
 */
static void erase_along(Controller *controller, Point from, Point to)
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = sqrtf(dx * dx + dy * dy);
    int steps = (int)ceilf(length / ERASE_SAMPLE_STEP);
    steps = steps < 1 ? 1 : steps;
    for (int s = 0; s <= steps; s++) {
        Point point = {from.x + dx * (float)s / (float)steps, from.y + dy * (float)s / (float)steps};
        for (int i = controller->scene->count - 1; i >= 0; i--) {
            Element *element = &controller->scene->items[i];
            if (!element->erasing && hit_element(element, point, ERASE_TOLERANCE)) {
                element->erasing = true;
                scene_mark_dirty(controller->scene, element_bounds(element));
            }
        }
    }
}

/**
 * @brief Moves the laser pointer dot and schedules the repaint of its old and new areas.
 * @param controller Controller to update.
 * @param position New dot center.
 */
static void move_laser_dot(Controller *controller, Point position)
{
    if (controller->laserDotVisible) {
        repaint_add(controller, laser_dot_bounds(controller->laserDot));
    }
    controller->laserDot = position;
    controller->laserDotVisible = true;
    repaint_add(controller, laser_dot_bounds(position));
}

/**
 * @brief Extends a trail and schedules the repaint of its old and new areas.
 * @param controller Controller to update.
 * @param trail Trail to extend.
 * @param position Raw pointer position.
 * @param nowMs Monotonic millisecond clock.
 */
static void extend_trail(Controller *controller, Trail *trail, Point position, uint32_t nowMs)
{
    repaint_add(controller, trail_bounds(trail));
    trail_add(trail, position, nowMs);
    repaint_add(controller, trail_bounds(trail));
}

/**
 * @brief Finds the topmost element under a point.
 * @param controller Controller whose scene is searched.
 * @param point Query point.
 * @return Element index or -1.
 */
static int pick_element(const Controller *controller, Point point)
{
    for (int i = controller->scene->count - 1; i >= 0; i--) {
        if (hit_element(&controller->scene->items[i], point, SELECT_TOLERANCE)) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Selects an element, repainting old and new decorations.
 * @param controller Controller to update.
 * @param index Element index or -1.
 */
static void select_element(Controller *controller, int index)
{
    clear_selection(controller);
    controller->selected = index;
    if (index >= 0) {
        repaint_add(controller, render_selection_bounds(&controller->scene->items[index]));
    }
}

/**
 * @brief Starts selection interaction: resize, move or deselect.
 * @param controller Controller to update.
 * @param sample Pointer data.
 */
static void select_down(Controller *controller, const PointerSample *sample)
{
    Scene *scene = controller->scene;
    if (controller->selected >= 0 && controller->selected < scene->count) {
        int handle = hit_handle(&scene->items[controller->selected], sample->position);
        if (handle != HIT_HANDLE_NONE) {
            controller->interaction = INTERACTION_RESIZE;
            controller->handle = handle;
            controller->dragStart = sample->position;
            controller->startBox = element_box(&scene->items[controller->selected]);
            controller->hasBefore = element_copy(&controller->before, &scene->items[controller->selected]);
            return;
        }
    }
    int picked = pick_element(controller, sample->position);
    select_element(controller, picked);
    if (picked >= 0) {
        controller->interaction = INTERACTION_MOVE;
        controller->dragStart = sample->position;
        controller->hasBefore = element_copy(&controller->before, &scene->items[picked]);
    }
}

/**
 * @brief Computes the box after dragging a handle.
 * @param box Starting box.
 * @param handle Handle index.
 * @param dx Horizontal drag distance.
 * @param dy Vertical drag distance.
 * @return Resized box with a minimum size enforced.
 */
static FloatRect dragged_box(FloatRect box, int handle, float dx, float dy)
{
    bool left = handle == 0 || handle == 6 || handle == 7;
    bool right = handle == 2 || handle == 3 || handle == 4;
    bool top = handle == 0 || handle == 1 || handle == 2;
    bool bottom = handle == 4 || handle == 5 || handle == 6;
    FloatRect out = box;
    if (left) {
        out.left = fminf(box.left + dx, box.right - MIN_BOX_SIZE);
    }
    if (right) {
        out.right = fmaxf(box.right + dx, box.left + MIN_BOX_SIZE);
    }
    if (top) {
        out.top = fminf(box.top + dy, box.bottom - MIN_BOX_SIZE);
    }
    if (bottom) {
        out.bottom = fmaxf(box.bottom + dy, box.top + MIN_BOX_SIZE);
    }
    return out;
}

/**
 * @brief Applies the current drag to the selected element.
 * @param controller Controller to update.
 * @param sample Pointer data.
 */
static void select_move(Controller *controller, const PointerSample *sample)
{
    Scene *scene = controller->scene;
    if (controller->selected < 0 || controller->selected >= scene->count) {
        return;
    }
    Element *item = &scene->items[controller->selected];
    repaint_add(controller, element_bounds(item));
    repaint_add(controller, render_selection_bounds(item));
    if (controller->interaction == INTERACTION_MOVE) {
        element_translate(item, sample->position.x - controller->dragStart.x,
                          sample->position.y - controller->dragStart.y);
        controller->dragStart = sample->position;
    } else if (controller->hasBefore) {
        float dx = sample->position.x - controller->dragStart.x;
        float dy = sample->position.y - controller->dragStart.y;
        FloatRect target = dragged_box(controller->startBox, controller->handle, dx, dy);
        if (replace_with_copy(scene, item, &controller->before)) {
            element_remap(item, controller->startBox, target);
        }
    }
    repaint_add(controller, element_bounds(item));
    repaint_add(controller, render_selection_bounds(item));
}

/**
 * @brief Commits a finished draft to the scene.
 * @param controller Controller to update.
 */
static void finish_draft(Controller *controller)
{
    Element *draft = &controller->draft;
    bool keep = true;
    if (draft->type == ELEMENT_PEN) {
        pen_finish(&controller->pen, draft);
    }
    if (draft->type == ELEMENT_RECTANGLE || draft->type == ELEMENT_ELLIPSE || draft->type == ELEMENT_DIAMOND) {
        keep = fabsf(draft->to.x - draft->from.x) >= MIN_SHAPE_SIZE ||
               fabsf(draft->to.y - draft->from.y) >= MIN_SHAPE_SIZE;
    } else if (draft->type == ELEMENT_LINE || draft->type == ELEMENT_ARROW) {
        keep = hypotf(draft->to.x - draft->from.x, draft->to.y - draft->from.y) >= MIN_SHAPE_SIZE;
    }
    repaint_add(controller, controller->draftBounds);
    if (keep && scene_add(controller->scene, draft)) {
        scene_commit(controller->scene);
        controller->toolFinished = true;
    } else {
        element_free(draft);
    }
    controller->hasDraft = false;
}

void controller_pointer_down(Controller *controller, const PointerSample *sample)
{
    if (!controller->scene || controller->interaction != INTERACTION_NONE) {
        return;
    }
    if (controller->editingText) {
        controller_text_commit(controller);
    }
    switch (controller->tool) {
    case TOOL_SELECT:
        select_down(controller, sample);
        break;
    case TOOL_ERASER:
        controller->interaction = INTERACTION_ERASE;
        controller->dragStart = sample->position;
        extend_trail(controller, &controller->eraser, sample->position, sample->timeMs);
        erase_along(controller, sample->position, sample->position);
        break;
    case TOOL_LASER:
        controller->interaction = INTERACTION_LASER;
        if (controller->laserPointer) {
            move_laser_dot(controller, sample->position);
        } else {
            trail_begin(&controller->laser);
            extend_trail(controller, &controller->laser, sample->position, sample->timeMs);
        }
        break;
    case TOOL_TEXT:
        controller->draft = element_make(ELEMENT_TEXT, &controller->style);
        controller->draft.from = sample->position;
        controller->draft.fontSize = font_size_for_width(controller->style.width);
        if (!element_set_text(&controller->draft, L"")) {
            element_free(&controller->draft);
            break;
        }
        controller->hasDraft = true;
        controller->editingText = true;
        controller->draftBounds = element_bounds(&controller->draft);
        repaint_add(controller, rect_inflate(controller->draftBounds, 8));
        break;
    default:
        if (begin_draft(controller, sample)) {
            controller->interaction = INTERACTION_DRAW;
            if (controller->draft.type == ELEMENT_PEN) {
                flush_pen_draft(controller);
            }
        }
        break;
    }
}

void controller_pointer_move(Controller *controller, const PointerSample *sample, bool down)
{
    if (controller->scene && controller->tool == TOOL_LASER && controller->laserPointer) {
        move_laser_dot(controller, sample->position);
        return;
    }
    if (!controller->scene || !down) {
        return;
    }
    switch (controller->interaction) {
    case INTERACTION_DRAW:
        if (controller->draft.type == ELEMENT_PEN) {
            float pressure = controller->style.pressure && sample->pressure >= 0.0f ? sample->pressure : PEN_PRESSURE_CONSTANT;
            if (pen_add(&controller->pen, &controller->draft, sample->position, pressure)) {
                flush_pen_draft(controller);
            }
        } else {
            repaint_add(controller, controller->draftBounds);
            update_shape_draft(controller, sample->position, sample->shift);
            controller->draftBounds = element_bounds(&controller->draft);
            repaint_add(controller, controller->draftBounds);
        }
        break;
    case INTERACTION_MOVE:
    case INTERACTION_RESIZE:
        select_move(controller, sample);
        break;
    case INTERACTION_ERASE:
        extend_trail(controller, &controller->eraser, sample->position, sample->timeMs);
        erase_along(controller, controller->dragStart, sample->position);
        controller->dragStart = sample->position;
        break;
    case INTERACTION_LASER:
        extend_trail(controller, &controller->laser, sample->position, sample->timeMs);
        break;
    case INTERACTION_NONE:
        break;
    }
}

void controller_pointer_up(Controller *controller, const PointerSample *sample)
{
    if (!controller->scene) {
        return;
    }
    switch (controller->interaction) {
    case INTERACTION_DRAW:
        finish_draft(controller);
        break;
    case INTERACTION_MOVE:
    case INTERACTION_RESIZE:
        if (controller->hasBefore && controller->selected >= 0 &&
            controller->selected < controller->scene->count) {
            scene_record_change(controller->scene, controller->selected, &controller->before);
            scene_commit(controller->scene);
        }
        controller->hasBefore = false;
        break;
    case INTERACTION_ERASE: {
        bool removed = false;
        for (int i = controller->scene->count - 1; i >= 0; i--) {
            if (controller->scene->items[i].erasing) {
                scene_remove(controller->scene, i);
                removed = true;
            }
        }
        if (removed) {
            scene_commit(controller->scene);
            controller->selected = -1;
        }
        repaint_add(controller, trail_bounds(&controller->eraser));
        trail_clear(&controller->eraser);
        controller->toolFinished = true;
        break;
    }
    case INTERACTION_LASER:
        controller->toolFinished = true;
        repaint_add(controller, trail_bounds(&controller->laser));
        trail_settle(&controller->laser, sample->timeMs);
        trail_release(&controller->laser);
        repaint_add(controller, trail_bounds(&controller->laser));
        break;
    case INTERACTION_NONE:
        break;
    }
    controller->interaction = INTERACTION_NONE;
}

void controller_style_preview(Controller *controller, const Style *style)
{
    if (!controller->scene || controller->selected < 0 || controller->selected >= controller->scene->count) {
        return;
    }
    Element *item = &controller->scene->items[controller->selected];
    if (!controller->hasStyleBefore) {
        if (!element_copy(&controller->styleBefore, item)) {
            return;
        }
        controller->hasStyleBefore = true;
    }
    repaint_add(controller, element_bounds(item));
    item->style = *style;
    if (item->type == ELEMENT_TEXT) {
        item->fontSize = font_size_for_width(style->width);
    }
    repaint_add(controller, element_bounds(item));
    repaint_add(controller, render_selection_bounds(item));
}

void controller_style_commit(Controller *controller)
{
    if (!controller->hasStyleBefore) {
        return;
    }
    controller->hasStyleBefore = false;
    if (controller->scene && controller->selected >= 0 && controller->selected < controller->scene->count) {
        scene_record_change(controller->scene, controller->selected, &controller->styleBefore);
        scene_commit(controller->scene);
    } else {
        element_free(&controller->styleBefore);
    }
}

void controller_delete_selection(Controller *controller)
{
    if (!controller->scene || controller->selected < 0 ||
        controller->selected >= controller->scene->count) {
        return;
    }
    repaint_add(controller, render_selection_bounds(&controller->scene->items[controller->selected]));
    scene_remove(controller->scene, controller->selected);
    scene_commit(controller->scene);
    controller->selected = -1;
}

void controller_apply_style_to_selection(Controller *controller, const Style *style)
{
    if (!controller->scene || controller->selected < 0 ||
        controller->selected >= controller->scene->count) {
        return;
    }
    Element *item = &controller->scene->items[controller->selected];
    Element before;
    if (!element_copy(&before, item)) {
        return;
    }
    repaint_add(controller, element_bounds(item));
    item->style = *style;
    if (item->type == ELEMENT_TEXT) {
        item->fontSize = font_size_for_width(style->width);
    }
    repaint_add(controller, element_bounds(item));
    repaint_add(controller, render_selection_bounds(item));
    scene_record_change(controller->scene, controller->selected, &before);
    scene_commit(controller->scene);
}

bool controller_text_input(Controller *controller, wchar_t character)
{
    if (!controller->editingText || !controller->hasDraft) {
        return false;
    }
    Element *draft = &controller->draft;
    size_t length = wcslen(draft->text);
    wchar_t *buffer = malloc((length + 2) * sizeof(wchar_t));
    if (!buffer) {
        return true;
    }
    memcpy(buffer, draft->text, length * sizeof(wchar_t));
    if (character == L'\b') {
        if (length == 0) {
            free(buffer);
            return true;
        }
        size_t removed = 1;
        if (length >= 2 && buffer[length - 1] >= 0xDC00 && buffer[length - 1] <= 0xDFFF &&
            buffer[length - 2] >= 0xD800 && buffer[length - 2] <= 0xDBFF) {
            removed = 2;
        }
        buffer[length - removed] = 0;
    } else if (character == L'\r' || character == L'\n') {
        buffer[length] = L'\n';
        buffer[length + 1] = 0;
    } else if (character >= 32) {
        buffer[length] = character;
        buffer[length + 1] = 0;
    } else {
        free(buffer);
        return false;
    }
    repaint_add(controller, rect_inflate(controller->draftBounds, 8));
    bool stored = element_set_text(draft, buffer);
    free(buffer);
    if (!stored) {
        return true;
    }
    controller->draftBounds = element_bounds(draft);
    repaint_add(controller, rect_inflate(controller->draftBounds, 8));
    return true;
}

void controller_text_commit(Controller *controller)
{
    if (!controller->editingText) {
        return;
    }
    controller->editingText = false;
    repaint_add(controller, rect_inflate(controller->draftBounds, 8));
    if (controller->hasDraft && controller->draft.text && controller->draft.text[0] &&
        scene_add(controller->scene, &controller->draft)) {
        scene_commit(controller->scene);
        controller->toolFinished = true;
    } else if (controller->hasDraft) {
        element_free(&controller->draft);
    }
    controller->hasDraft = false;
}

bool controller_animate(Controller *controller, uint32_t nowMs)
{
    bool alive = false;
    Trail *trails[2] = {&controller->laser, &controller->eraser};
    for (int i = 0; i < 2; i++) {
        if (trails[i]->count > 0) {
            repaint_add(controller, trail_bounds(trails[i]));
            if (trail_prune(trails[i], nowMs)) {
                alive = true;
            }
            repaint_add(controller, trail_bounds(trails[i]));
        }
    }
    return alive;
}

void controller_draw_overlays(const Controller *controller, Canvas *canvas, const RECT *clip,
                              uint32_t nowMs)
{
    if (controller->hasDraft) {
        render_element(canvas, &controller->draft, clip);
        if (controller->editingText) {
            render_text_caret(canvas, &controller->draft, clip);
        }
    }
    if (controller->tool == TOOL_SELECT && controller->scene && controller->selected >= 0 &&
        controller->selected < controller->scene->count &&
        controller->interaction != INTERACTION_ERASE) {
        render_selection(canvas, &controller->scene->items[controller->selected], clip);
    }
    trail_render(&controller->eraser, canvas, clip, nowMs);
    trail_render(&controller->laser, canvas, clip, nowMs);
    if (controller->laserDotVisible && controller->tool == TOOL_LASER && controller->laserPointer) {
        laser_dot_render(canvas, clip, controller->laserDot);
    }
}

void controller_add_transient_bounds(const Controller *controller, Dirty *extent)
{
    if (controller->hasDraft) {
        dirty_add(extent, rect_inflate(element_bounds(&controller->draft), TRANSIENT_MARGIN));
    }
    if (controller->scene && controller->selected >= 0 && controller->selected < controller->scene->count) {
        const Element *selected = &controller->scene->items[controller->selected];
        dirty_add(extent, rect_inflate(render_selection_bounds(selected), TRANSIENT_MARGIN));
    }
    if (controller->laser.count > 0) {
        dirty_add(extent, rect_inflate(trail_bounds(&controller->laser), TRANSIENT_MARGIN));
    }
    if (controller->eraser.count > 0) {
        dirty_add(extent, rect_inflate(trail_bounds(&controller->eraser), TRANSIENT_MARGIN));
    }
    if (controller->laserDotVisible) {
        dirty_add(extent, rect_inflate(laser_dot_bounds(controller->laserDot), TRANSIENT_MARGIN));
    }
}

void controller_take_dirty(Controller *controller, Dirty *repaint, Dirty *direct)
{
    if (controller->scene) {
        Dirty sceneDirty;
        scene_take_dirty(controller->scene, &sceneDirty);
        if (!sceneDirty.empty) {
            repaint_add(controller, sceneDirty.rect);
        }
    }
    *repaint = controller->repaint;
    *direct = controller->direct;
    dirty_reset(&controller->repaint);
    dirty_reset(&controller->direct);
}

bool controller_take_tool_finished(Controller *controller)
{
    bool finished = controller->toolFinished;
    controller->toolFinished = false;
    return finished;
}

const Element *controller_selection(const Controller *controller)
{
    if (!controller->scene || controller->selected < 0 || controller->selected >= controller->scene->count) {
        return NULL;
    }
    return &controller->scene->items[controller->selected];
}

bool controller_is_busy(const Controller *controller)
{
    return controller->interaction != INTERACTION_NONE || controller->editingText;
}
