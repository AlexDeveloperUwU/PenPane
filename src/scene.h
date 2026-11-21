/**
 * @file scene.h
 * @brief An ordered list of elements with unlimited undo and redo.
 *
 * Every mutation is recorded as an operation inside the current step. A step groups operations
 * that the user perceives as one action (for example erasing three elements). Steps are
 * unbounded; memory is the only limit.
 */
#ifndef INK_SCENE_H
#define INK_SCENE_H

#include <stdbool.h>

#include "element.h"

/** @brief Kinds of recorded operation. */
typedef enum OpKind {
    OP_ADD,
    OP_REMOVE,
    OP_REPLACE
} OpKind;

/** @brief One recorded change at a list index. */
typedef struct Op {
    OpKind kind;
    int index;
    Element before;
    Element after;
} Op;

/** @brief A group of operations undone or redone together. */
typedef struct Step {
    Op *ops;
    int count;
    int capacity;
} Step;

/** @brief Elements plus history. */
typedef struct Scene {
    Element *items;
    int count;
    int capacity;
    Step *undo;
    int undoCount;
    int undoCapacity;
    Step *redo;
    int redoCount;
    int redoCapacity;
    Step pending;
    Dirty dirty;
} Scene;

/**
 * @brief Prepares an empty scene.
 * @param scene Scene to initialize.
 */
void scene_init(Scene *scene);

/**
 * @brief Frees every element and every history step.
 * @param scene Scene to release.
 */
void scene_destroy(Scene *scene);

/**
 * @brief Appends an element and records it in the pending step. Takes ownership of the element's memory.
 * @param scene Target scene.
 * @param element Element to add; zeroed on success and left untouched on failure, so the caller still owns it then.
 * @return true on success; on false the scene and its pending step are unchanged.
 */
bool scene_add(Scene *scene, Element *element);

/**
 * @brief Removes the element at an index and records it in the pending step.
 * @param scene Target scene.
 * @param index Index to remove.
 */
void scene_remove(Scene *scene, int index);

/**
 * @brief Records that the element at an index changed from a saved copy to its current value.
 *
 * Call after mutating scene->items[index] in place. Ownership of the saved copy transfers to the history.
 * @param scene Target scene.
 * @param index Index of the changed element.
 * @param before Copy of the element before the change; zeroed on success.
 */
void scene_record_change(Scene *scene, int index, Element *before);

/**
 * @brief Marks pixels belonging to an element as needing repaint.
 * @param scene Target scene.
 * @param bounds Pixel rectangle to repaint.
 */
void scene_mark_dirty(Scene *scene, RECT bounds);

/**
 * @brief Finishes the pending step and pushes it on the undo stack, discarding the redo stack.
 * @param scene Target scene.
 */
void scene_commit(Scene *scene);

/**
 * @brief Removes every element as one undoable step.
 * @param scene Target scene.
 */
void scene_clear(Scene *scene);

/**
 * @brief Undoes the last step.
 * @param scene Target scene.
 * @return true when something was undone.
 */
bool scene_undo(Scene *scene);

/**
 * @brief Redoes the last undone step.
 * @param scene Target scene.
 * @return true when something was redone.
 */
bool scene_redo(Scene *scene);

/**
 * @brief Tells whether undo is available.
 * @param scene Scene to query.
 * @return true when the undo stack is not empty.
 */
bool scene_can_undo(const Scene *scene);

/**
 * @brief Tells whether redo is available.
 * @param scene Scene to query.
 * @return true when the redo stack is not empty.
 */
bool scene_can_redo(const Scene *scene);

/**
 * @brief Returns and resets the accumulated dirty region.
 * @param scene Scene to query.
 * @param region Receives the region; empty when nothing changed.
 */
void scene_take_dirty(Scene *scene, Dirty *region);

#endif
