/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shape_text.h"

#include <math.h>
#include <wchar.h>

#define DOUBLE_CLICK_MS 400u
#define DOUBLE_CLICK_DISTANCE 6.0f
#define REPAINT_MARGIN 8

/**
 * @brief Finds the topmost shape whose box contains a point.
 * @param scene Scene to search.
 * @param point Query point.
 * @return Element index, or -1.
 */
static int shape_at(const Scene *scene, Point point)
{
    for (int i = scene->count - 1; i >= 0; i--) {
        const Element *element = &scene->items[i];
        if (!element_is_shape(element)) {
            continue;
        }
        FloatRect box = element_box(element);
        if (point.x >= box.left && point.x <= box.right && point.y >= box.top && point.y <= box.bottom) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Tells whether a press repeats the previous one closely enough to be a double click.
 * @param controller Controller holding the previous press.
 * @param sample The new press.
 * @return true for a double click.
 */
static bool is_double_click(const Controller *controller, const PointerSample *sample)
{
    float dx = sample->position.x - controller->lastDownPosition.x;
    float dy = sample->position.y - controller->lastDownPosition.y;
    return controller->hasLastDown && sample->timeMs - controller->lastDownMs <= DOUBLE_CLICK_MS &&
           hypotf(dx, dy) <= DOUBLE_CLICK_DISTANCE;
}

/**
 * @brief Selects a shape and makes typed characters go to its label.
 * @param controller Controller to update.
 * @param index Index of the shape in the scene.
 */
static void begin_edit(Controller *controller, int index)
{
    Element *item = &controller->scene->items[index];
    if (!item->text && !element_set_text(item, L"")) {
        return;
    }
    if (!element_copy(&controller->textBefore, item)) {
        return;
    }
    controller->hasTextBefore = true;
    if (controller->selected != index) {
        controller_deselect(controller);
        controller->selected = index;
    }
    controller->editingText = true;
    controller->editIndex = index;
    dirty_add(&controller->repaint, element_selection_bounds(item));
    dirty_add(&controller->repaint, rect_inflate(element_bounds(item), REPAINT_MARGIN));
}

bool shape_text_pointer_down(Controller *controller, const PointerSample *sample)
{
    int index = -1;
    if (controller->tool == TOOL_TEXT) {
        index = shape_at(controller->scene, sample->position);
    } else if (controller->tool == TOOL_SELECT) {
        if (is_double_click(controller, sample)) {
            index = shape_at(controller->scene, sample->position);
        }
        controller->lastDownMs = sample->timeMs;
        controller->lastDownPosition = sample->position;
        controller->hasLastDown = index < 0;
    }
    if (index < 0) {
        return false;
    }
    begin_edit(controller, index);
    return true;
}

void shape_text_commit(Controller *controller)
{
    int index = controller->editIndex;
    Scene *scene = controller->scene;
    controller->editingText = false;
    controller->editIndex = -1;
    if (!controller->hasTextBefore) {
        return;
    }
    controller->hasTextBefore = false;
    if (scene && index >= 0 && index < scene->count) {
        Element *item = &scene->items[index];
        dirty_add(&controller->repaint, rect_inflate(element_bounds(item), REPAINT_MARGIN));
        const wchar_t *now = item->text ? item->text : L"";
        const wchar_t *was = controller->textBefore.text ? controller->textBefore.text : L"";
        if (wcscmp(now, was) != 0) {
            scene_record_change(scene, index, &controller->textBefore);
            scene_commit(scene);
            controller->toolFinished = true;
        }
    }
    element_free(&controller->textBefore);
}
