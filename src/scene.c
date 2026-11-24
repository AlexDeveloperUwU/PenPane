/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Frees every element copy held by a step and the step's operation array.
 * @param step Step to release.
 */
static void step_free(Step *step)
{
    for (int i = 0; i < step->count; i++) {
        element_free(&step->ops[i].before);
        element_free(&step->ops[i].after);
    }
    free(step->ops);
    memset(step, 0, sizeof *step);
}

/**
 * @brief Appends an operation to a step.
 * @param step Step to extend.
 * @param operation Operation to append, copied by value (element memory ownership moves).
 * @return true on success.
 */
static bool step_push(Step *step, const Op *operation)
{
    if (step->count == step->capacity) {
        int capacity = step->capacity ? step->capacity * 2 : 4;
        Op *grown = realloc(step->ops, sizeof(Op) * (size_t)capacity);
        if (!grown) {
            return false;
        }
        step->ops = grown;
        step->capacity = capacity;
    }
    step->ops[step->count++] = *operation;
    return true;
}

/**
 * @brief Pushes a step on a stack of steps.
 * @param stack Pointer to the array pointer.
 * @param count Pointer to the element count.
 * @param capacity Pointer to the capacity.
 * @param step Step to push; ownership moves to the stack.
 * @return true on success.
 */
static bool stack_push(Step **stack, int *count, int *capacity, const Step *step)
{
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 16;
        Step *replacement = realloc(*stack, sizeof(Step) * (size_t)grown);
        if (!replacement) {
            return false;
        }
        *stack = replacement;
        *capacity = grown;
    }
    (*stack)[(*count)++] = *step;
    return true;
}

/**
 * @brief Frees all steps in a stack.
 * @param stack Pointer to the array pointer.
 * @param count Pointer to the element count.
 * @param capacity Pointer to the capacity.
 */
static void stack_free(Step **stack, int *count, int *capacity)
{
    for (int i = 0; i < *count; i++) {
        step_free(&(*stack)[i]);
    }
    free(*stack);
    *stack = NULL;
    *count = 0;
    *capacity = 0;
}

/**
 * @brief Drops all undo and redo history, including the pending step.
 *
 * Used when an out-of-memory failure leaves the scene changed without a matching record, so that
 * undo can never replay a history that no longer describes the scene.
 * @param scene Scene whose history is discarded.
 */
static void history_discard(Scene *scene)
{
    step_free(&scene->pending);
    stack_free(&scene->undo, &scene->undoCount, &scene->undoCapacity);
    stack_free(&scene->redo, &scene->redoCount, &scene->redoCapacity);
}

/**
 * @brief Inserts an element copy into the item list at an index.
 * @param scene Target scene.
 * @param index Insert position, clamped to the list size.
 * @param element Element whose memory moves into the list.
 * @return true on success.
 */
static bool items_insert(Scene *scene, int index, const Element *element)
{
    if (scene->count == scene->capacity) {
        int capacity = scene->capacity ? scene->capacity * 2 : 64;
        Element *grown = realloc(scene->items, sizeof(Element) * (size_t)capacity);
        if (!grown) {
            return false;
        }
        scene->items = grown;
        scene->capacity = capacity;
    }
    if (index < 0) {
        index = 0;
    } else if (index > scene->count) {
        index = scene->count;
    }
    memmove(scene->items + index + 1, scene->items + index, sizeof(Element) * (size_t)(scene->count - index));
    scene->items[index] = *element;
    scene->count++;
    return true;
}

/**
 * @brief Removes an element from the list without freeing its memory.
 * @param scene Target scene.
 * @param index Index to remove.
 */
static void items_erase(Scene *scene, int index)
{
    memmove(scene->items + index, scene->items + index + 1,
            sizeof(Element) * (size_t)(scene->count - index - 1));
    scene->count--;
}

void scene_init(Scene *scene)
{
    memset(scene, 0, sizeof *scene);
    dirty_reset(&scene->dirty);
}

void scene_destroy(Scene *scene)
{
    for (int i = 0; i < scene->count; i++) {
        element_free(&scene->items[i]);
    }
    free(scene->items);
    stack_free(&scene->undo, &scene->undoCount, &scene->undoCapacity);
    stack_free(&scene->redo, &scene->redoCount, &scene->redoCapacity);
    step_free(&scene->pending);
    memset(scene, 0, sizeof *scene);
}

void scene_mark_dirty(Scene *scene, RECT bounds)
{
    dirty_add(&scene->dirty, bounds);
}

bool scene_add(Scene *scene, Element *element)
{
    Op operation = {OP_ADD, scene->count, {0}, {0}};
    if (!element_copy(&operation.after, element)) {
        return false;
    }
    if (!step_push(&scene->pending, &operation)) {
        element_free(&operation.after);
        return false;
    }
    if (!items_insert(scene, scene->count, element)) {
        scene->pending.count--;
        element_free(&operation.after);
        return false;
    }
    scene_mark_dirty(scene, element_bounds(element));
    memset(element, 0, sizeof *element);
    return true;
}

void scene_remove(Scene *scene, int index)
{
    Op operation = {OP_REMOVE, index, scene->items[index], {0}};
    operation.before.erasing = false;
    scene_mark_dirty(scene, element_bounds(&scene->items[index]));
    bool recorded = step_push(&scene->pending, &operation);
    if (!recorded) {
        element_free(&scene->items[index]);
    }
    items_erase(scene, index);
    if (!recorded) {
        history_discard(scene);
    }
}

void scene_record_change(Scene *scene, int index, Element *before)
{
    Op operation = {OP_REPLACE, index, *before, {0}};
    if (!element_copy(&operation.after, &scene->items[index])) {
        element_free(before);
        history_discard(scene);
        return;
    }
    operation.before.erasing = false;
    operation.after.erasing = false;
    if (step_push(&scene->pending, &operation)) {
        memset(before, 0, sizeof *before);
    } else {
        element_free(&operation.after);
        element_free(before);
        history_discard(scene);
    }
}

void scene_commit(Scene *scene)
{
    if (scene->pending.count == 0) {
        return;
    }
    if (stack_push(&scene->undo, &scene->undoCount, &scene->undoCapacity, &scene->pending)) {
        memset(&scene->pending, 0, sizeof scene->pending);
        stack_free(&scene->redo, &scene->redoCount, &scene->redoCapacity);
    } else {
        history_discard(scene);
    }
}

void scene_clear(Scene *scene)
{
    for (int i = scene->count - 1; i >= 0; i--) {
        scene_remove(scene, i);
    }
    scene_commit(scene);
}

/**
 * @brief Applies the inverse (undo) or the forward (redo) effect of a step.
 * @param scene Target scene.
 * @param step Step to apply.
 * @param undoing true to invert the step, false to replay it.
 * @return false when an allocation failed and the step was only partly applied.
 */
static bool step_apply(Scene *scene, const Step *step, bool undoing)
{
    bool complete = true;
    for (int n = 0; n < step->count; n++) {
        const Op *operation = &step->ops[undoing ? step->count - 1 - n : n];
        OpKind kind = operation->kind;
        bool add = (kind == OP_ADD && !undoing) || (kind == OP_REMOVE && undoing);
        bool remove = (kind == OP_ADD && undoing) || (kind == OP_REMOVE && !undoing);
        if (add) {
            Element copy;
            const Element *source = kind == OP_ADD ? &operation->after : &operation->before;
            if (!element_copy(&copy, source)) {
                complete = false;
            } else if (!items_insert(scene, operation->index, &copy)) {
                element_free(&copy);
                complete = false;
            } else {
                scene_mark_dirty(scene, element_bounds(&copy));
            }
        } else if (remove && operation->index < scene->count) {
            scene_mark_dirty(scene, element_bounds(&scene->items[operation->index]));
            element_free(&scene->items[operation->index]);
            items_erase(scene, operation->index);
        } else if (kind == OP_REPLACE && operation->index < scene->count) {
            Element copy;
            const Element *source = undoing ? &operation->before : &operation->after;
            if (element_copy(&copy, source)) {
                scene_mark_dirty(scene, element_bounds(&scene->items[operation->index]));
                scene_mark_dirty(scene, element_bounds(&copy));
                element_free(&scene->items[operation->index]);
                scene->items[operation->index] = copy;
            } else {
                complete = false;
            }
        }
    }
    return complete;
}

bool scene_undo(Scene *scene)
{
    if (scene->undoCount == 0) {
        return false;
    }
    Step step = scene->undo[--scene->undoCount];
    bool complete = step_apply(scene, &step, true);
    if (!complete || !stack_push(&scene->redo, &scene->redoCount, &scene->redoCapacity, &step)) {
        step_free(&step);
        history_discard(scene);
    }
    return true;
}

bool scene_redo(Scene *scene)
{
    if (scene->redoCount == 0) {
        return false;
    }
    Step step = scene->redo[--scene->redoCount];
    bool complete = step_apply(scene, &step, false);
    if (!complete || !stack_push(&scene->undo, &scene->undoCount, &scene->undoCapacity, &step)) {
        step_free(&step);
        history_discard(scene);
    }
    return true;
}

bool scene_can_undo(const Scene *scene)
{
    return scene->undoCount > 0;
}

bool scene_can_redo(const Scene *scene)
{
    return scene->redoCount > 0;
}

void scene_take_dirty(Scene *scene, Dirty *region)
{
    *region = scene->dirty;
    dirty_reset(&scene->dirty);
}
