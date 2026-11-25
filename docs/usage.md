# Usage

## Start and stop

Run `PenPane.exe`. It lives in the notification area. Starting it a second time toggles drawing on the running instance. `Ctrl+Alt+Shift+Esc` always quits.

## Two modes

- **Desktop:** transparent layer over whatever is on screen. Turn drawing off and clicks pass through while annotations stay visible. Optional auto-fade clears them after a delay (`auto_fade_seconds` in the config, off by default).
- **Whiteboard:** an opaque dark canvas. Desktop and whiteboard drawings are kept separately.

## Tools

The application starts on the select tool, so nothing is drawn until you choose a drawing tool.

| Tool | Key | Notes |
|---|---|---|
| Selection | V | click to select, drag to move, handles to resize, Delete to remove |
| Rectangle | R | Shift makes a square |
| Ellipse | O | Shift makes a circle |
| Diamond | D | Shift makes an equal-sided diamond |
| Line and arrow | L | Shift snaps to 15 degrees; the properties panel chooses no head, one head or two |
| Pen | P or X | pressure when the tablet reports it, speed based otherwise |
| Laser | K | fading red trail, leaves nothing behind; after you lift the pen every stroke fades tail first, short ones too |
| Text | T | Enter adds a line, Esc finishes |
| Eraser | E | removes whole elements |
| Tool lock | Q | keep the tool after each stroke (on by default); turn it off to return to the select tool after every stroke, with every tool (pen, eraser and laser included) |

The keys 1 to 9 and 0 select the tools by their place in the toolbar (the small number on each button), so they follow the order you set in the settings. Hidden tools are skipped.

Ctrl+Z undoes, Ctrl+Y or Ctrl+Shift+Z redoes. History is unlimited.

## Floating tools and a second screen

The toolbar, the properties panel and the undo and redo panel are three separate windows. Drag any of them by its grip to another monitor. Choose the drawing screen and the tools screen from the tray menu. By default the tools are excluded from screen capture, so people watching a shared screen do not see them. Each one shows as a small handle until you click it, and an opened one folds back into its handle about a second after the pen or pointer leaves it. The pin button next to the grip keeps that panel open; the setting is remembered per panel. With drawing off only the toolbar handle is shown: click it to open the toolbar, then use its drawing button to start drawing.

## Hide, clear and settings

The undo and redo panel holds, in order: undo and redo; copy and save as PNG; the **mode button**, which switches between desktop and whiteboard (it shows the mode you are in; `toggle_mode` does the same); and the eye and trash buttons below. The gear is the second to last button of the toolbar, before the exit button, and works while drawing is off:

- **Eye:** (desktop mode only) hides every drawing so you can use the desktop, and shows them again. Nothing is deleted. The panels stay open in both states. Bind `toggle_drawings` to a pen button for a quick toggle.
- **Trash:** clears the canvas after asking "Clear canvas?". Undo brings the drawings back. The `clear` action still clears without asking.
- **Gear:** opens the settings window, which has four tabs: **General** (auto-hide, "hide panels from screenshots", pastel color row, debug log, and the two reset buttons), **Toolbar** (move each tool up or down, switch tools off to leave them out of the toolbar), **Shortcuts** (one extra shortcut per tool) and **Pen keys** (types F13 to F16 into the tablet driver's key box). A tool's extra shortcut is added to its built-in keys, which stay: on the Shortcuts tab click "Add" next to the tool, then press the key or pen button the way you want to use it (single, double or long press), or, if you cannot press it, click one of the F13 to F18 buttons that appear above the list; the cross removes it. It is the same as the tray menu's Remap buttons and the `bind = ... tool_pen` lines in the ini. "Reset all options" puts every option back to its default and keeps your button bindings, custom colors and panel positions. "Reset menu positions" puts the toolbar and the panels back at their default places. Changes are saved to `PenPane.ini`.

The settings, the "Clear canvas" question and the color picker are separate windows: opening one never closes another.

## Colors

The stroke row has five presets (light gray, red, green, blue, yellow) and a round color picker button at its end. The picker opens a small window with a brightness square, a hue strip and a hex field; the stroke (or the selected element) changes while you drag, and the drag keeps following the pointer outside the window. Click the hex field to type a color: three or six hex digits (Ctrl+V pastes), Enter applies, Esc cancels. Turn on "Pastel color row" in the settings for a second row of pastel colors. Add your own with `custom_color = #RRGGBB` lines in `PenPane.ini` (see [configuration.md](configuration.md)); they appear as an extra row.

## Debug log

If something does not work as expected, switch on "Write a debug log" in the settings (or `debug_log = true`). PenPane then writes `PenPane.log` next to the executable with one short line per event: start-up, configuration warnings, mode changes, window failures, display and lock events, tray notices and crashes. Pointer movement, drawings and typed text are never logged. The log is off by default and costs one flag test per event while off; while on it writes each line straight to the file without keeping anything in memory, and the file is capped at 1 MB (the previous file is kept as `PenPane.log.1`). Switch it off again when you are done.

## Buttons on the pen

Configure the pen buttons in the tablet driver to send rarely used keys, for example F13 and F14 (the "Pen buttons" row in the settings types them into the driver's key box for you; see [configuration.md](configuration.md)), then bind actions with the tray menu: Remap buttons, choose an action, then press the button the way you want to use it (single, double or long press). Default placeholders: F13 single undo, F13 double toggle overlay, F14 single next tool, F14 double toggle mode, F14 long clear. Details in [configuration.md](configuration.md).

## Copy and save as PNG

Use the copy and save buttons in the undo and redo panel, bind `copy_png` or `save_png`, or use the tray menu. The desktop drawing is exported with transparency, the whiteboard with its background. Save opens the system save dialog and writes the file in the background.

Two switches in Settings, General tab, change what is exported; both are on by default. **Export the full screen** exports the whole draw screen in either mode instead of only the painted area. **Add desktop to exports** (desktop mode only) puts the drawing over a screenshot of what is behind it, without the overlay or the tool panels, so the PNG is opaque. The matching config keys are `export_full_screen` and `export_background`.
