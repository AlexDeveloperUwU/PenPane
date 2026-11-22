# Usage

## Start and stop

Run `penpane.exe`. It lives in the notification area. Starting it a second time toggles drawing on the running instance. `Ctrl+Alt+Shift+Esc` always quits.

## Two modes

- **Desktop:** transparent layer over whatever is on screen. Turn drawing off and clicks pass through while annotations stay visible. Optional auto-fade clears them after a delay (`auto_fade_seconds` in the config, off by default).
- **Whiteboard:** an opaque dark canvas. Desktop and whiteboard drawings are kept separately.

## Tools

| Tool | Key | Notes |
|---|---|---|
| Selection | V or 1 | click to select, drag to move, handles to resize, Delete to remove |
| Rectangle | R or 2 | Shift makes a square |
| Ellipse | O or 4 | Shift makes a circle |
| Arrow | A or 5 | Shift snaps to 15 degrees |
| Line | L or 6 | Shift snaps to 15 degrees |
| Pen | P, X or 7 | pressure when the tablet reports it, speed based otherwise |
| Laser | K | fading red trail, leaves nothing behind |
| Text | T or 8 | Enter adds a line, Esc finishes |
| Eraser | E or 0 | removes whole elements |
| Tool lock | Q | keep the tool after each stroke (on by default) |

Ctrl+Z undoes, Ctrl+Y or Ctrl+Shift+Z redoes. History is unlimited.

## Floating tools and a second screen

The toolbar, the properties panel and the undo and redo panel are three separate windows. Drag any of them by its grip to another monitor. Choose the drawing screen and the tools screen from the tray menu. By default the tools are excluded from screen capture, so people watching a shared screen do not see them. Each one shows as a small handle until you click it, and an opened one folds back into its handle about a second after the pen or pointer leaves it. The pin button next to the grip keeps that panel open; the setting is remembered per panel. With drawing off only the toolbar handle is shown: click it to open the toolbar, then use its drawing button to start drawing.

## Buttons on the pen

Configure the pen buttons in the tablet driver to send rarely used keys, for example F13 and F14, then bind actions with the tray menu: Remap buttons, choose an action, then press the button the way you want to use it (single, double or long press). Default placeholders: F13 single undo, F13 double toggle overlay, F14 single next tool, F14 double toggle mode, F14 long clear. Details in [configuration.md](configuration.md).

## Copy and save as PNG

Use the copy and save buttons in the undo and redo panel, bind `copy_png` or `save_png`, or use the tray menu. The desktop drawing is exported with transparency, the whiteboard with its background. Save opens the system save dialog and writes the file in the background.
