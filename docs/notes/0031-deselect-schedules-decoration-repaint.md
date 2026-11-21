# 0031. Dropping the selection always repaints its decoration

- Status: accepted
- Date: 2026-11-21

## Problem

Undo, redo and clear reset the selection index directly. The selection box and handles are drawn above the scene and are larger than the element, so the scene repaint did not cover them and a ghost box stayed on screen after every undo of a selected element. Ghosts accumulated.

## Decision

`controller_deselect` drops the selection and schedules a repaint of the decoration bounds. The undo, redo and clear actions call it before they change the scene.

## Why

The controller already repaints a decoration when the tool changes or the user clicks away; the actions simply bypassed that path. One entry point keeps the rule in one place.

## Alternatives rejected

- Repainting the whole surface after every undo: simple, but a full-screen repaint for a small change.
- Keeping the selection across undo: it needs index remapping when elements are added or removed, which is more state than the benefit justifies.

## Consequences

Undo still clears the selection. Any new code that must clear the selection should call `controller_deselect` and not write the index.
