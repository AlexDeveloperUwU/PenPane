# 0020. Three separate island windows instead of one UI window

- Status: accepted
- Date: 2026-11-21

## Problem

The toolbar, the properties panel and the history panel float over the overlay and must be draggable to any monitor, independently of each other. We had to decide between one window that spans all of them and one small window per island.

## Decision

Each island is its own layered top-level window (`WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`) sized to its content plus a soft shadow margin. Windows are never activated: `WM_MOUSEACTIVATE` returns `MA_NOACTIVATE`.

## Why

A single window large enough to hold three islands that may sit on different monitors would span the whole virtual desktop, so it would need a full-screen pixel buffer, and it would swallow clicks in the gaps. Small windows cost a few hundred kilobytes in total, only claim the pixels they cover, move with a plain `SetWindowPos`, and a hidden island simply has no window on screen.

## Alternatives rejected

- One window spanning the virtual desktop: tens of megabytes of pixels and extra click-through handling for the gaps.
- One window that is moved as a group: the islands could not be placed independently, which Excalidraw users expect.

## Consequences

Three window procedures share one implementation and one class. Hit testing outside an island's content rectangle (the shadow margin) returns `HTTRANSPARENT` so clicks fall through to the overlay. Z-order must be kept above the overlay with `ui_raise`.
