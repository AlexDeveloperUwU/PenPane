# User interface

The UI is three small floating islands in the Excalidraw dark style, plus a notification-area icon. Colors, sizes and shortcuts come from `docs/excalidraw-reference.md` and `src/theme.h`. The public interface is `src/ui.h` and `src/tray.h`; everything else is private to the module.

## Files

| File | Role |
|---|---|
| `src/ui.c` | Windows, state machine, hit testing, dragging, auto-hide, event emission |
| `src/ui_layout.c` | Builds the list of items of each island from `UiModel` and a scale factor |
| `src/ui_paint.c` | Shadow, body, buttons, swatches, switch, slider, handle |
| `src/icons.c` | Stroke icons on a 16 unit grid, drawn with the rasterizer |
| `src/shapes_ui.c` | Reusable scratch mask and one-call fills, strokes and arcs |
| `src/text.c` | GDI text rendered into coverage masks |
| `src/tray.c` | Notification icon, context menu, autostart registry value |
| `tests/ui_preview.c` | Visual harness |

## Layout

All metrics are at 100 percent and multiplied by one scale factor read in `island_read_scale` (`GetDpiForWindow / 96`).

- Island: background `#232329`, radius 8, padding 4, soft three-layer shadow in a 20 px margin around the content.
- Buttons: 36 px, 16 px icons, radius 8, hover `#2e2d39`, pressed slightly lighter, selected `#403e6a` with icon `#e0dfff`, disabled icons at 30 percent. Keybinding hints are 10 px in `#7a7a7a` at the bottom right.
- Pin button: the second element of every island, right after the grip. Unpinned it is a thin outlined push pin tilted by 45 degrees (`ICON_PIN_OFF`); pinned it is an upright solid pin on the selected button background (`ICON_PIN`).
- Toolbar, left to right: drag grip, pin, divider, tool lock (`Q`), divider, nine tools in `tool.h` order with hints `1 2 4 5 6 7 K 8 0`, divider, desktop or whiteboard mode, drawing on or off, emergency exit.
- Properties panel (width 211, padding 12) starts with a header row of grip and pin, then the sections chosen by `UiModel.sections`: Stroke (five presets, divider, current color), Background (five presets, the first drawn as a checkerboard), Stroke width and Stroke style (three buttons each), Pressure (switch), Opacity (slider 0 to 100 in steps of 10 with a value label). A selected preset gets a 2 px ring in `#d0ccff`.
- History panel: grip, pin, divider, undo, redo; disabled buttons do not react to hover or clicks.

## Behavior

- Islands never take focus; `WM_MOUSEACTIVATE` returns `MA_NOACTIVATE`. Pen and touch arrive as mouse messages.
- Dragging: press on the grip or any empty part of an island. The position is clamped so at least a 24 px strip stays on the nearest monitor. `UI_EVENT_PANEL_MOVED` is sent on release and carries the content's top-left corner (without the shadow margin).
- A button fires on release when the pointer is still over it. The opacity slider fires while dragging, once per change.
- Hover repaints only the old and new item and presents only that rectangle.
- `ui_place_defaults`: toolbar centered at the top with an 8 px margin, properties panel at the left 16 px below the toolbar row, history panel at the bottom left 16 px from the edges. Saved placements are honored unless `forceAll` is set; with `forceAll` each new position is reported through `UI_EVENT_PANEL_MOVED`.
- `ui_apply_settings` (an addition to the header) applies changed auto-hide, delay, pinned islands and capture exclusion at runtime.
- The pin button reports `UI_EVENT_PANEL_PINNED` (panel, index 1 pinned or 0 unpinned) on release; the UI has already updated its own settings, and `app.c` stores the value in the configuration and saves it.

### Open on click, close when left

An island that is not open shows as a 28 px handle anchored at its top-left corner. The handle shows the active tool (toolbar, the "drawing off" glyph while not drawing), the stroke color (properties) or the undo arrow (history). Hovering a handle only draws its hover highlight. Pressing and releasing it without dragging more than 4 px expands the island, shifted if needed so it fits the monitor; dragging moves it as before. Newly shown islands start as handles unless they are pinned.

An expanded island that is not pinned collapses to its handle `autoHideMs` after the pointer leaves it (default 1200 ms, never below 200 ms). It does not collapse while the pointer is inside, a button is held, or a drag or slider is active. A pinned island never collapses by itself; with `UiSettings.autoHide` off every island behaves as pinned while the handles still work. The timer is one-shot and exists only while an island is expanded, unpinned and the pointer is elsewhere, so an idle program receives no timer messages.

### Not drawing

`ui_set_visible(ui, true, false)` shows only the toolbar (handle or open, as above). Click the handle to open it and use its drawing button to start drawing; the handle itself no longer toggles drawing. A pinned toolbar stays open across drawing on and off. The properties and history islands are hidden while drawing is off and return, open if pinned, when drawing starts. `ui_set_visible(ui, false, ...)` hides everything. Hidden islands are hidden windows without a canvas.

## Capture exclusion

All three windows get `WDA_EXCLUDEFROMCAPTURE` when `UiSettings.excludeFromCapture` is set. `ui_set_capture_excluded` returns false if the OS refuses; see note 0021.

## Tray

A runtime-generated pen glyph (32-bit ARGB icon created with `CreateIconIndirect`, drawn from the same geometry as the application icon in `res/penpane.ico`). The tooltip reads "PenPane" followed by the version from `src/version.h`. Left click toggles drawing. The right-click menu holds Drawing on/off, Mode (Desktop, Whiteboard), Draw screen, Tool panels screen, Hide tools from screen capture, Auto-hide tools, Remap buttons (every action, underscores shown as spaces; "Cancel learning" while learning), Copy drawing as PNG, Reload config, Open config file, Start with Windows and Quit. The icon uses the version 4 callback protocol: `tray_handle_message` reacts to `NIN_SELECT`, `NIN_KEYSELECT` and `WM_CONTEXTMENU`. The menu is shown with `SetForegroundWindow`, `TrackPopupMenu(TPM_RETURNCMD)` and a posted `WM_NULL` so it dismisses correctly. `autostart_get` and `autostart_set` use the value `PenPane` under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` and compare against the quoted path of the running executable.

## Memory and CPU

- Pixels exist only for shown islands (roughly 200 KB for the toolbar, less for the others).
- One shared scratch mask and one shared text surface; no per-draw allocations beyond text masks.
- No periodic timers; repaint happens only on change.

## Limits

- Scale is read per island window and re-read on `WM_DPICHANGED`; the default placement uses the scale of the monitor the islands were created on.
- Capture exclusion needs Windows 10 version 2004 or later.
- The collapsed toolbar handle sits at the toolbar's left end, not at its center.
- A handle near the right or bottom edge expands shifted, so the pointer can end up outside the island; an unpinned island then collapses after the delay.
- The current-color swatch is display only; there is no color picker.
- The native tray menu cannot be styled.

## Preview harness

`tests/ui_preview.c` shows the islands over a dark background and applies their events to a local model. Options: `--tool N`, `--sections HEX`, `--autohide MS` (islands start as handles and open on click), `--pin HEX` (bit 0 toolbar, 1 properties, 2 history), `--idle`, `--lock`, `--whiteboard`, `--noundo`, `--demo`, `--exclude`, `--quit MS`. Build:

    clang -std=c11 -municode -D_WIN32_WINNT=0x0A00 -mwindows -Isrc tests/ui_preview.c src/ui.c src/ui_layout.c src/ui_paint.c src/icons.c src/shapes_ui.c src/text.c src/raster.c src/canvas.c src/style.c src/tool.c -o build/ui_preview.exe -luser32 -lgdi32 -lshell32

Screenshots can be taken with PowerShell `Graphics.CopyFromScreen`; moving the real cursor drives hover and click states.
