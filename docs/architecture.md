# Architecture

PenPane is a single native executable written in C11 against Win32. There is no UI toolkit, no web runtime and no managed code. Everything on screen is produced by a small software rasterizer and shown through layered windows.

## Module map

| Layer | Modules | Responsibility |
|---|---|---|
| Pixels | `canvas`, `raster`, `mask_scratch`, `text`, `png` | Premultiplied DIB surface, anti-aliased shapes through signed distance fields, one reusable coverage buffer, GDI text turned into coverage masks, uncompressed PNG encoder |
| Model | `element`, `scene`, `style`, `geometry`, `grow` | Editable elements, ordered scene, unlimited undo and redo as grouped operations, palettes, rectangles and dirty regions, array growth helper |
| Behavior | `controller`, `pen`, `hittest`, `trail`, `tool` | Pointer and keyboard interaction for every tool, freehand model, hit testing, fading laser and eraser trails |
| Drawing | `render` | Draws elements, selection decoration, text caret, clipped to a rectangle |
| Input | `gesture`, `hotkeys`, `config`, `actions` | Single, double and long gestures, global hotkeys, mouse buttons, learn mode, plain-text configuration |
| Shell | `app`, `overlay`, `ui`, `tray`, `clipboard`, `jobs`, `log`, `main` | State machine, overlay window, floating islands and dialogs, tray icon, PNG export, background work, optional event log |

`log` is off by default. A `LOG(...)` call tests one flag; when the log is on it formats one line on the stack and writes it straight to the file (no heap, no buffer kept), rotating at 1 MB. It is called from the UI thread only and records events, never pointer movement.

None of `gesture`, `pen`, `hittest`, `scene` and `controller` includes a Windows header directly. The Model and Behavior layers still reach `windows.h` indirectly, because `geometry.h` (included by `element.h`) uses `RECT` and `POINT`, and `canvas.h` (included by `render.h`, which `controller.h` includes) uses GDI handles. That is accepted for now; only `gesture`, `actions`, `tool`, `style` and `prefs` are free of Windows types. Known boundary debt:

- `element.c` measures text through `text.h` (GDI), so element bounds depend on the Pixels layer.
- `controller.c` calls `render.h` to draw its own overlays (draft and trail), although the data flow below says it never draws to the screen.
- `config.c` includes `windows.h` for file I/O; `config.h` is free of the shell: the panel preference types live in `prefs.h`.

`tests/` covers `config`, `controller`, `gesture`, `scene` (which also exercises `pen` and `hittest` for rectangles) and `trail`.

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

One thread owns the windows, the hooks and one message loop, blocking `GetMessage`. The window that owns timers, hotkeys and the tray icon is a hidden top-level popup, because message-only windows get no broadcasts (note 0070). Slow work, which means configuration writes and PNG encoding, runs on a short-lived worker thread so the low-level mouse hook is never kept waiting (note 0075). Timers exist only while needed:

- animation timer (16 ms) only while a laser or eraser trail is visible,
- hotkey polling timer (12 ms) only while a bound key is held or a gesture is pending,
- auto-hide timers, one-shot, only while a panel is expanded and idle,
- optional annotation fade timers, one-shot,
- the tray icon retry timer, one-shot with backoff, only while the shell has not accepted the icon (note 0071).

## Memory

The surface is one 32-bit buffer. While drawing or on the whiteboard it is the size of the drawing monitor; in passthrough it covers only the extent of the annotations (note 0060). It exists only while the overlay window exists. The reusable mask buffer is released together with the overlay and trimmed after a flush when it grows beyond 256 KB. The working set is trimmed at idle moments: before the message loop, when drawing is off and when a trail animation ends (note 0061). Text services are switched off when only plain keyboard layouts are installed (note 0062). Measured numbers are in [measurements.md](measurements.md).

## Decisions

See [docs/notes](notes/index.md) for one record per non-obvious decision.
