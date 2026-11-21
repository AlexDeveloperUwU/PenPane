# 0060. The passthrough surface covers only the extent of the annotations

- Status: accepted
- Date: 2026-11-21

## Problem

With drawing off and annotations on screen, the overlay still owned a surface the size of the monitor (14 MB at 2560x1376). Measured with the real executable: working set 43.6 MB, private working set 15.2 MB, private bytes 15.8 MB, for two strokes. The working set was larger than in drawing mode because Windows also mapped a second 13.8 MB section for the layered window while it was click-through.

## Decision

- `Canvas` has an origin. Drawing coordinates stay relative to the monitor; `canvas_fill`, `canvas_blend_coverage` and `canvas_area` translate them, so renderers never learn where the surface sits.
- `overlay_set_surface` makes the surface cover a rectangle of the monitor and reallocates only when the size changes. `overlay_present` places the window at the monitor corner plus the origin.
- `wanted_surface` in `app.c` returns the whole monitor while drawing or on the whiteboard, and the union of the element bounds (one pixel when empty) in passthrough.
- `app_flush` recomputes the rectangle in passthrough when something repaints, so undo, redo and clear from a hotkey grow or shrink the surface without leaving the passthrough state.

## Why

The window is click-through (`WS_EX_TRANSPARENT`), so its size has no effect on input. Nothing outside the annotations is visible, so nothing outside them needs pixels.

## Alternatives rejected

- Keeping the full surface: simplest, costs 15 MB for as long as annotations stay on screen.
- Tiles or a sparse surface: layered windows take one bitmap, so tiling means several windows and more state for no extra saving over the bounding box.
- The same trick in drawing mode: input capture needs every pixel hit-testable (note 0002), so the full surface is required there. See note 0068.

## Consequences

Measured, passthrough with two strokes: working set 43.6 to 1.6 MB, private working set 15.2 to 0.4 MB, private bytes 15.8 to 2.7 MB. Annotations spread over the whole screen keep a near-full surface, so the saving shrinks with their extent. Switching from passthrough back to drawing now allocates the full surface again, which costs about 15 ms more than before (measured 40 ms before, 57 ms after for that switch). Pixels on screen are unchanged: the same strokes cover the same pixels in passthrough, after undo and after redo.
