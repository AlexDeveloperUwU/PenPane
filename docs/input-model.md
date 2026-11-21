# Input model: capture versus passthrough

The highest-risk part of an overlay is deciding who receives the pointer. PenPane uses one layered window with per-pixel alpha.

## Passthrough

The window has `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`. `WS_EX_TRANSPARENT` makes the window invisible to hit testing, so every click reaches the application below. Annotations stay visible because the surface is still presented.

## Capture

Removing `WS_EX_TRANSPARENT` is not enough. A layered window with per-pixel alpha is also click-through wherever a pixel has alpha 0, so an empty overlay would still let clicks fall through. In drawing mode the backdrop is filled with alpha 1 (about 0.4 percent, invisible), which keeps every pixel hit-testable. In whiteboard mode the backdrop is opaque.

Verified end to end with synthetic input: with drawing on, a click on the application below does not reach it and a drag leaves a stroke; with drawing off, the same click reaches the application while the stroke stays visible.

## Focus

Keyboard shortcuts and text input need focus. In drawing mode the overlay drops `WS_EX_NOACTIVATE` and takes the foreground (using a thread-input attach so Windows allows it from a hotkey). The previously focused window is remembered and restored when drawing ends. In passthrough the overlay never takes focus.

## Pointer sources

- Windows Ink pointer messages (`WM_POINTER*`) for pens and touch, with pressure when the digitizer reports it.
- Plain mouse messages for tablets that behave like an absolute mouse and for real mice. Pressure is then simulated from speed.

## Safety

- The emergency hotkey Ctrl+Alt+Shift+Esc is registered separately from the binding table and cannot be removed by configuration.
- The tray menu and the toolbar always offer drawing off and exit.
- A crash filter hides the overlay and the OS removes every window when the process ends.

## Known limits

- Windows owned by elevated processes can receive clicks above a non-elevated overlay only if the overlay is also elevated.
- Exclusive full-screen games bypass the compositor and cannot be drawn over.
- `WDA_EXCLUDEFROMCAPTURE` (hiding the tools from screen sharing) needs Windows 10 version 2004 or later; some capture tools ignore it.
