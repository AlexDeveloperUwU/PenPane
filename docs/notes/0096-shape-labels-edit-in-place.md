# 0096. Shape labels are edited in place and opened by the controller

- Status: accepted
- Date: 2026-11-25

## Problem

Rectangles, ellipses and diamonds could not carry text. A label has to be typed somewhere, undone as one step, and the way to start typing must work with a mouse, a pen and a touch screen alike.

## Decision

A shape's label is its own `text` field, drawn centered in the shape's box with the shape's font, size and stroke color. Typing edits that element in the scene directly, instead of a separate draft element, and the whole edit is recorded as one undoable change when it ends. Two gestures start it: pressing inside a shape with the text tool, and a second press with the select tool within 400 ms and 6 px of the first. The controller detects the second press itself from the pointer timestamps (`src/shape_text.c`). The label grows the element's repaint bounds when it is wider than the shape.

## Why

The draft element of free text cannot be reused: it is not in the scene, so the shape would have to be removed, rebuilt and re-added, and undo would then need two steps. Editing in place keeps one element and one history entry. Timing in the controller works for every input device the program already handles; a window double-click message would need a separate path for the pen and would not fire for a second press on a different window class.

## Alternatives rejected

- A separate text element positioned over the shape: moving or deleting the shape would leave the text behind, and resizing would not keep it centered.
- Window double-click messages: they depend on window class styles and system settings, and do not cover pen input.
- Wrapping the label to the shape width: the text engine has no wrapping, and a wrapped layout needs a width model for ellipses and diamonds. Lines break only where the user presses Enter.

## Consequences

With the text tool, a press inside any shape edits that shape, so free text cannot be started inside a shape with that tool; it can be started outside and the shape's label is for text that belongs to the shape. Long labels overflow the shape. Free text elements still cannot be edited after they are committed.
