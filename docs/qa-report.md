# QA report

Date: 2026-11-21. Build: `scripts\build.ps1` (clang, `-Os -flto`), 105 KB. Screen 2560x1376 at 100 percent, one display. The real `build\penpane.exe` was driven with synthetic mouse and keyboard input (user32 `SetCursorPos`, `mouse_event`, `SendInput`), a WinForms window as the application underneath, and `CopyFromScreen` screenshots analysed by pixel counts and by eye.

## Bugs found and fixed

| # | Bug | Root cause | Fix |
|---|---|---|---|
| 1 | The test script could not compile the programs that link `app.c` (`LoadCursorW` type errors) | `scripts/test.ps1` did not define `UNICODE`, which `-municode` defines for the main build | `scripts/test.ps1` defines `UNICODE`/`_UNICODE` and links the Windows libraries |
| 2 | `tool_lock` defaulted to true in code, false in the documentation and test; `test_config` aborted | `config_defaults` | Default is false (note 0032); the owner may prefer the opposite, see the note |
| 3 | A pen stroke stopped about 14 px short of the release point on fast strokes | Streamlining lags the last point and nothing added the final position | `pen_finish` (note 0030) |
| 4 | Ghost selection boxes and handles stayed on screen after undo, redo or clear of a selected element, and accumulated | Actions reset the selection index without scheduling a repaint of its decoration | `controller_deselect` (note 0031) |
| 5 | Hardening: after drawing is switched off the overlay could end up beneath another topmost window that regained focus | Only focus was restored | The overlay is re-asserted as topmost when leaving draw mode |

Regression tests: `tests/test_controller.c` (12 headless tests with a real in-memory canvas) covers pen end position, tap dot, deselect repaint, move undo and cancel, resize and undo, eraser mark and remove and undo and cancel, shift constraints, tiny-shape discard, text editing, laser transience and selection style undo. `scripts\test.ps1` passes all four programs.

## Verified working (real exe)

- Pen (opaque, opacity 40 percent, pressure on and off, slow and fast, a self-overlapping loop keeps one uniform alpha); a tap makes a dot.
- Rectangle, ellipse, line and arrow, including the live preview while dragging; shift constraints (unit-tested).
- Eraser: marked while dragging, removed on release, undo restores, redo removes, fast drags erase everything crossed.
- Selection: click select, move, resize, Delete, Backspace, undo and redo of each.
- Text: typing, Backspace, Enter newline, Esc commit, click-away commit, empty text discarded, undo and redo.
- Laser: trail visible while dragging, gone within about one second, no permanent ink, nothing in the undo history, animation timer stops (idle CPU 0 afterwards).
- Tool lock on and off (Q key and an F17 binding), shortcuts R O A L P K T E V Q, Ctrl+Z, Ctrl+Y, Delete.
- Toolbar buttons (tools, mode, drawing toggle, exit), properties panel (stroke colour, fill, width, stroke style, pressure toggle, opacity slider) and history undo and redo buttons.
- Desktop and whiteboard keep separate scenes and undo stacks across repeated switching; the whiteboard backdrop is #121212.
- Clear (F14 long) and its undo; copy PNG (F15) leaves both a registered PNG format and CF_DIB on the clipboard.
- Hotkeys from the default bindings via `SendInput`: F13 single (undo), F13 double (toggle overlay), F14 single (next tool), F14 double (mode), F14 long (clear), plus extra bindings F15 to F21 (copy, laser, lock, redo, colour, width).
- Emergency exit Ctrl+Alt+Shift+Esc exits the process and leaves nothing on screen, including in whiteboard mode with a text edit open.
- Second launch toggles drawing; focus moves to the overlay when drawing starts and returns to the previous window when it stops; typing and clicking reach the window underneath in passthrough; the islands never take focus.
- Auto fade (2 s): strokes fade and are removed, the timer dies and memory is released; toggling drawing back on before the fade keeps them.
- Learn mode, exercised against the real hooks with injected keys: single, double, long, Ctrl plus key, Esc cancel, emergency combination cancel, explicit cancel; hooks are gone afterwards and normal handling resumes.
- Display change, resume and session unlock: the handlers were driven by posting `WM_DISPLAYCHANGE`, `WM_POWERBROADCAST` and `WM_WTSSESSION_CHANGE` to the running instance, also mid-stroke. No crash, no lost annotations.

## Observations, not changed

- Undo, redo and clear drop the selection. Excalidraw keeps it; dropping it is simpler and consistent.
- After drawing one element with lock off the new element is not selected and the properties panel hides, because the tool is select with no selection.
- Toggle laser returns to the tool that was active when the laser was entered, which is select after a one-shot drawing, because the automatic return to select records no previous tool.
- The pen tap dot is small (about 4 px at medium width). This is a visual constant in `pen.c`, left to the visual tuning work.
- A launch of a second instance during the first 100 ms of the first instance's startup is ignored.
- The overlay cannot rise above a window that is itself topmost and activated later, except at the next state change.
- Not testable here: real pen pressure, pen barrel buttons, multiple monitors, DPI other than 100 percent, the native tray context menu.

## Resources

Measured with `TotalProcessorTime` deltas over 30 seconds per state.

| State | CPU delta over 30 s | Working set | Private |
|---|---|---|---|
| Hidden (toolbar handle only), after warm-up | 0 ms | 10.0 MB | 1.5 MB |
| First 30 s after launch (creating the islands) | 47 ms | 10.0 MB | 1.5 MB |
| Drawing, empty scene | 0 ms | 30.4 MB | 16.3 MB |
| Drawing after laser faded | 0 ms | 31.5 MB | 16.2 MB |
| Drawing with annotations | 0 ms | 30.3 MB | 16.2 MB |
| Passthrough with annotations | 0 ms | 43.4 MB | 16.0 MB |
| Whiteboard drawing | 0 ms | 30.1 MB | 16.1 MB |
| Whiteboard, drawing off (overlay destroyed) | 0 ms | 16.3 MB | 2.3 MB |
| Desktop, drawing off, empty scene (overlay destroyed) | 0 ms | 16.3 MB | 2.3 MB |
| After eraser (8 s) | 0 ms | 30.4 MB | 16.2 MB |

Hovering the pointer over the overlay costs no measurable CPU.
