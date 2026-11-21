# 0078. A slider drag is a single undo step

- Status: accepted
- Date: 2026-11-21

## Problem

Dragging the opacity slider emitted one event per step, and each event recorded an undo step on the selected element. Ten steps in one drag meant ten presses of undo to get back to the value before the drag, and the history panel filled with near-identical entries.

## Decision

While a drag is running, the controller changes the selected element directly and keeps one copy of it from before the first change (`controller_style_preview`). When the interface reports the end of the drag, a new event sent on button release or on capture loss, `controller_style_commit` records a single step from that copy to the final state. Any `controller_cancel`, which runs before undo, redo, clear, tool changes and hiding the overlay, commits a pending edit first, so nothing is lost and the order of history stays correct.

## Why

The undo model records operations between two states of an element, so a drag only needs its first and last state. Committing on the release event keeps the number of steps equal to the number of gestures the user made.

## Alternatives rejected

- Merging into the previous undo step when the style changes again: needs a rule for when two steps belong together and would also merge two separate drags.
- Debouncing with a timer: a pause in the middle of a drag would split it.

## Consequences

Other style controls still record one step per click, which matches one gesture per step. A new `UI_EVENT_SLIDER_END` kind exists for any future slider.
