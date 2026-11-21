# 0022. Islands collapse to a handle and free their pixels when hidden

- Status: accepted
- Date: 2026-11-21

## Problem

The tool must idle at zero CPU and a small memory footprint, yet the islands should get out of the way while the user draws, and come back without a visible delay.

## Decision

- A hidden island keeps its window object but has no canvas. The 32-bit surface is created when the island is shown and destroyed when it is hidden or resized.
- With auto-hide on, an expanded island arms a one-shot `SetTimer` whenever the pointer is not over it. When the timer fires it is killed and the island collapses to a 28 px handle with a glyph. No timer exists while the island is collapsed or while the pointer is over it. Hovering the handle expands it again.
- When drawing is off, only the toolbar handle stays on screen and clicking it reports `ACTION_TOGGLE_OVERLAY`.
- Hover and press changes repaint only the affected items and present only that dirty rectangle.

## Why

Pixels are the dominant memory cost, so holding them only while an island is visible keeps the idle working set small. A repeating timer would wake the process for nothing; a one-shot timer started only on leave events means the idle process receives no messages at all.

## Alternatives rejected

- A periodic poll of the cursor position: constant wake-ups.
- Keeping canvases while hidden: roughly 200 KB per island for no visible gain.
- Fade animations: they need a repeating timer while they run and add little on an overlay used for drawing.

## Consequences

Showing an island repaints it fully (a few thousand pixels of shadow and shapes, well under a millisecond). Auto-hide state is per island, so the properties panel may collapse while the toolbar stays open.
