/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file shape_text.h
 * @brief Editing the text label of a rectangle, ellipse or diamond in place.
 */
#ifndef PENPANE_SHAPE_TEXT_H
#define PENPANE_SHAPE_TEXT_H

#include "controller.h"

/**
 * @brief Starts editing the label of the shape under a pointer press, when the press asks for it.
 *
 * With the text tool, pressing inside a shape edits that shape's label. With the select tool, a second press
 * close in time and place to the previous one does the same. Typing then goes through controller_text_input.
 * @param controller Controller to drive.
 * @param sample The pointer press.
 * @return true when the press started a label edit and must not be handled further.
 */
bool shape_text_pointer_down(Controller *controller, const PointerSample *sample);

/**
 * @brief Finishes a label edit, recording one undoable step when the text changed.
 * @param controller Controller whose editIndex is not negative.
 */
void shape_text_commit(Controller *controller);

#endif
