# Architecture

Ink is a single native executable written in C11 against Win32. There is no UI toolkit, no web runtime and no managed code. Everything on screen is produced by a small software rasterizer and shown through layered windows.

## Module map

| Layer | Modules | Responsibility |
|---|---|---|
| Pixels | `canvas`, `raster`, `mask_scratch`, `text` | Premultiplied DIB surface, anti-aliased shapes through signed distance fields, one reusable coverage buffer, GDI text turned into coverage masks |
| Model | `element`, `scene`, `style`, `geometry` | Editable elements, ordered scene, unlimited undo and redo as grouped operations, palettes, rectangles and dirty regions |
| Behavior | `controller`, `pen`, `hittest`, `trail`, `tool` | Pointer and keyboard interaction for every tool, freehand model, hit testing, fading laser and eraser trails |
| Drawing | `render` | Draws elements, selection decoration, text caret, clipped to a rectangle |
| Input | `gesture`, `hotkeys`, `config`, `actions` | Single, double and long gestures, global hotkeys, mouse buttons, learn mode, plain-text configuration |
| Shell | `app`, `overlay`, `ui`, `tray`, `clipboard`, `main` | State machine, overlay window, floating islands, tray icon, PNG export |

`model`, `gesture`, `config`, `pen` and `hittest` do not depend on Windows state and are covered by tests in `tests/`.

## Data flow

1. A pointer message reaches the overlay window procedure in `app.c`. Pen, touch and mouse paths all produce a `PointerSample`.
2. `controller` interprets it for the active tool and mutates the scene or its own draft. It never draws to the screen. It reports two regions: `repaint` (clear and redraw) and `direct` (already drawn, only present).
3. `app_flush` repaints the `repaint` region with `render_scene` plus the controller overlays, then calls `UpdateLayeredWindow` with the union as dirty rectangle.
4. Pen strokes with an opaque color are drawn incrementally: only the newest segments are blended onto the surface, so a long stroke costs the same per event as a short one.

## States

| State | Window | Input | Notes |
|---|---|---|---|
| Hidden | none | normal | no surface allocated; only the toolbar handle remains |
| Passthrough | overlay visible, `WS_EX_TRANSPARENT`, pixels with alpha 0 | reaches apps below | desktop annotations remain visible |
| Drawing | overlay visible, capture on | taken by the overlay | backdrop alpha 1 keeps empty pixels hit-testable |

The whiteboard is always drawing; turning drawing off hides it. Desktop and whiteboard have separate scenes and histories.

## Threads and timers

One thread, one message loop, blocking `GetMessage`. Timers exist only while needed:

- animation timer (16 ms) only while a laser or eraser trail is visible,
- hotkey polling timer (12 ms) only while a bound key is held or a gesture is pending,
- auto-hide timers, one-shot, only while a panel is expanded and idle,
- optional annotation fade timers, one-shot.

## Memory

The surface is one 32-bit buffer. While drawing or on the whiteboard it is the size of the drawing monitor; in passthrough it covers only the extent of the annotations (note 0060). It exists only while the overlay window exists. The reusable mask buffer is released together with the overlay and trimmed after a flush when it grows beyond 256 KB. The working set is trimmed at idle moments: before the message loop, when drawing is off and when a trail animation ends (note 0061). Text services are switched off when only plain keyboard layouts are installed (note 0062). Measured numbers are in [measurements.md](measurements.md).

## Decisions

See [docs/notes](notes/index.md) for one record per non-obvious decision.
