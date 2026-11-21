# 0043. Selection look: thin outline and four corner handles

- Status: accepted
- Date: 2026-11-21

## Problem

PenPane drew a 1.5 px outline with rounded corners and eight handles. Excalidraw's selection looks lighter.

## Decision

The outline is 1 px, square cornered, in `#b3afff`, snapped to whole pixels. Handles are 8 px rounded squares with a 1 px outline and the canvas colour as fill, drawn only at the four corners. `render_selection_box` keeps its signature with the padding changed to 3.5 + width / 2, so `hit_handle` still finds all eight handles.

## Why

Reading the interactive canvas of excalidraw.com for a selected ellipse gave outline colour 179, 175, 255 at half alpha across two pixel rows, corner handles 8 to 10 px wide with fill 18, 18, 18, no side handles and handle centres 4.5 px outside a width 2 element.

## Alternatives rejected

Keeping eight handles: more visual weight than the reference. Removing the side handles from hit testing: out of scope and it would remove a resize feature.

## Consequences

Side handles stay active but invisible. If that confuses users, the loop in `render_selection` can draw them again. The rotation circle is not drawn because PenPane has no rotation.
