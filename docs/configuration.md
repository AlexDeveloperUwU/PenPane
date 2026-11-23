# Configuration

The application reads one plain-text file. Every key is optional: a missing key keeps its default, and a line that is unknown or invalid is skipped (the rest of the file still applies). The application rewrites the whole file when it saves a change, so comments you add by hand are not preserved after a save.

## File format

- One `key = value` per line. Keys are case-insensitive. Blanks around the key, the `=` and the value are ignored.
- A line starting with `#` is a comment. Blank lines are ignored. Comments after a value are not supported.
- UTF-8 with or without a byte-order mark, and LF or CRLF line endings, are accepted. Only ASCII is meaningful.
- Booleans accept `true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0`.
- A value outside the documented range invalidates the line; the previous (default) value is kept.
- A file that does not exist is created with the defaults, except that an `ink.ini` next to the executable (the file name of the former product name) is copied to `PenPane.ini` once and then read (note 0092). A file that exists but cannot be read, for example because another program has it locked, is left exactly as it is: the application keeps the settings it already has, shows a notice, and does not save anything until a reload reads the file successfully.

## Settings

| Key | Default | Range or values | Meaning |
|---|---|---|---|
| `double_press_ms` | `300` | 50 to 2000 | Maximum gap between the two presses of a double gesture. Also the delay added to a single gesture on keys that have a double binding. |
| `long_press_ms` | `600` | 200 to 5000 | How long a key or button must stay down for a long gesture. |
| `default_tool` | `select` | `select`, `rectangle`, `ellipse`, `diamond`, `arrow`, `line`, `pen`, `laser`, `text`, `eraser` | Tool active when the application starts. The default is the select tool, so nothing is drawn by accident. |
| `hidden_tools` | `diamond` | comma separated tool names | Tools left out of the toolbar. Their shortcuts still work. At least one tool must stay visible. The settings window writes this line for you. |
| `tool_order` | `select, pen, eraser, text, rectangle, ellipse, diamond, arrow, line, laser` | comma separated tool names | Order of the tool buttons in the toolbar. Tools you leave out follow the listed ones in the default order; unknown or repeated names reject the whole line. The number keys follow this order: `1` is the first visible tool, `2` the second, and so on up to `0` for the tenth. The letter shortcuts do not change. |
| `default_stroke_index` | `0` | 0 to 4 | Index of the stroke color preset used at start. |
| `palette_pastel` | `false` | boolean | Show a second row of pastel stroke colors in the properties panel. The settings window has the same switch. |
| `export_full_screen` | `true` | boolean | Export the whole draw screen instead of only the painted area (copy and save as PNG, both modes). The settings window has the same switch. |
| `export_background` | `true` | boolean | Desktop mode only: put the exported drawing over a screenshot of the desktop behind it, without the overlay or tool panels. The settings window has the same switch. |
| `debug_log` | `false` | boolean | Write `PenPane.log` next to the executable: one timestamped line per start-up, configuration result, mode change, window failure, display, lock or resume event, tray notice and crash. No pointer movement, drawings or typed text. Capped at 1 MB with the older file kept as `PenPane.log.1`. The settings window has the same switch. |
| `custom_color` | none | `#RRGGBB` | Adds one swatch to a "custom" row under the stroke colors. Repeat the line for more, up to 18 (extra lines are skipped). The leading `#` is optional. |
| `default_fill_index` | `0` | 0 to 4 | Index of the background preset used at start; 0 is transparent. |
| `default_width_index` | `1` | 0 to 2 | Index of the stroke width preset used at start: 0 thin, 1 medium, 2 bold. |
| `auto_fade_seconds` | `0` | 0 to 3600 | Seconds after which finished strokes fade out. 0 keeps strokes until cleared. |
| `draw_monitor` | `0` | 0 to 15 | Monitor that holds the drawing surface. 0 is the primary monitor. |
| `ui_monitor` | `0` | 0 to 15 | Monitor that holds the toolbar and panels. 0 is the primary monitor. |
| `tool_lock` | `true` | boolean | Keep the selected tool after drawing one element instead of returning to the select tool. |
| `laser_pointer` | `false` | boolean | The laser tool shows a dot that follows the pointer instead of a fading trail. Also a button in the properties panel while the laser tool is active. |
| `ui_auto_hide` | `true` | boolean | Collapse an open toolbar or panel to its handle after the pointer leaves it. When `false`, panels stay open once opened, as if all were pinned. |
| `ui_auto_hide_ms` | `1200` | 500 to 60000 | Time after the pointer leaves an unpinned panel before it collapses. |
| `ui_exclude_from_capture` | `true` | boolean | Keep the toolbar and panels out of screen recordings and screenshots. |
| `toolbar_pos` | not set | `x,y` | Saved top-left screen position of the toolbar. Negative values are valid on monitors left of or above the primary one. |
| `properties_pos` | not set | `x,y` | Saved position of the properties panel. |
| `history_pos` | not set | `x,y` | Saved position of the history panel. |
| `toolbar_pinned` | `false` | boolean | Keep the toolbar open. Written when you press its pin button. |
| `properties_pinned` | `false` | boolean | Keep the properties panel open. |
| `history_pinned` | `false` | boolean | Keep the history panel open. |
| `reset_bindings` | not set | boolean | When `true`, removes every binding collected so far, including the defaults. Saved files always start their bindings with it. |
| `bind` | see below | `trigger gesture action` | Assigns an action to a trigger and gesture. May appear many times. |

The three position keys are written when you move a panel and are absent until then. The three pinned keys are written with every save and apply on reload.

## Bindings

```
bind = <trigger> <gesture> <action>
```

The three parts are separated by blanks. A later `bind` for the same trigger and gesture replaces the earlier one. At most 64 bindings are kept, and at most 16 different triggers.

### Default bindings

The defaults are placeholders meant to be changed:

```
bind = F13 single undo
bind = F13 double toggle_overlay
bind = F14 single next_tool
bind = F14 double toggle_mode
bind = F14 long clear
```

A file that has no `reset_bindings` line adds to these. To remove a default, put `reset_bindings = true` before your own `bind` lines and list everything you want. A file written by the application always contains `reset_bindings = true` followed by the complete table, so it does not depend on the defaults of a later version.

### Triggers

A trigger is optional modifier prefixes joined by `+`, then one key or mouse button.

| Part | Names |
|---|---|
| Modifiers | `Ctrl` (or `Control`), `Alt`, `Shift`, `Win` |
| Letters and digits | `A` to `Z`, `0` to `9` |
| Function keys | `F1` to `F24` |
| Named keys | `Esc`, `Space`, `Enter`, `Tab`, `Home`, `End`, `PageUp`, `PageDown`, `Insert`, `Delete`, `Left`, `Right`, `Up`, `Down` |
| Numeric keypad | `Numpad0` to `Numpad9` |
| Any other key | `VK_0xNN`, one or two hex digits of the Windows virtual-key code, from `01` to `FE` |
| Mouse buttons | `MButton`, `XButton1`, `XButton2` (the middle button and the two side buttons) |

Examples: `F13`, `Ctrl+Alt+F14`, `Shift+Numpad5`, `VK_0xC0`, `XButton1`. The left and right mouse buttons and the wheel cannot be bound.

A key that types text, which means a letter, a digit, `Space`, `Esc`, `Enter`, `Tab`, Backspace or a punctuation key, is accepted only together with `Ctrl`, `Alt` or `Win`. `Shift` alone does not count. A line such as `bind = A single undo` is reported as invalid and skipped, because a global hotkey takes the key away from every other program.

Keyboard triggers are registered with the system as global hotkeys. If another application already owns the same combination, that trigger cannot be registered; the application reports it with the system's reason and the other triggers keep working. Pick a different key or change the other application.

Only 16 different triggers can be tracked. When the file uses more, the application warns and ignores the extra ones.

A bound mouse button is consumed by this application and no longer reaches other programs, so avoid binding `MButton` or the side buttons if you rely on their normal behavior.

### Gestures

| Gesture | When it fires |
|---|---|
| `single` | One short press. Fires immediately on press, unless the same trigger also has a `double` binding, in which case it fires when the double window has passed without a second press. With a `long` binding but no `double`, it fires on release of a short press. |
| `double` | Two presses less than `double_press_ms` apart. Fires on the second press. |
| `long` | The key or button is held for `long_press_ms`. Fires while still held; releasing afterwards does nothing more. |

Holding a key down does not repeat the action: auto-repeat is ignored.

Because a `double` binding makes the single action wait for the window to expire, only give a trigger a `double` binding if you want that delay. Triggers with only `single`, or `single` plus `long`, respond without that delay.

### Actions

`toggle_overlay`, `toggle_mode`, `tool_select`, `tool_rectangle`, `tool_ellipse`, `tool_arrow`, `tool_line`, `tool_pen`, `tool_laser`, `tool_text`, `tool_eraser`, `tool_diamond`, `tool_highlighter`, `next_tool`, `previous_tool`, `cycle_color`, `cycle_width`, `undo`, `redo`, `clear`, `toggle_laser`, `toggle_tool_lock`, `copy_png`, `save_png`, `toggle_drawings`, `ask_clear`, `open_settings`, `emergency_exit`.

Names are case-insensitive.

## Learn mode

Learn mode lets you assign a button without typing its name. From the settings, choose an action and start learning, then do the gesture you want on the key or button:

- Press once and release quickly: `single`.
- Press twice within the double window: `double`.
- Press and hold for at least the long-press time, then release: `long`.
- Hold modifiers while pressing to include them, for example `Ctrl` with `F14`.

While learning, the captured keys and the middle and side mouse buttons are swallowed so they do not act on other applications, and the modifier keys are swallowed too. The left and right mouse buttons keep working so you can click the interface. Press `Esc` on its own to cancel (the `Ctrl+Alt+Shift+Esc` emergency combination also cancels). Learning ends by itself after 15 seconds without a captured gesture. A single is confirmed only after the double window has passed, so expect a short pause before it is reported.

Keys that type text cannot be learned or bound on their own (see Triggers); learning such a key ends with a notice instead of a binding.

## Pen buttons with an XP-Pen tablet

Pen barrel buttons are not keys, so the tablet driver has to turn them into keystrokes. The recommended mapping is two unused function keys that no other program uses:

1. Open the XP-Pen driver and select your pen or tablet.
2. In the pen-button settings, set the lower barrel button to send the keyboard key `F13` and the upper barrel button to send `F14`. Most keyboards have no F13 to F24 keys, so the driver's key box cannot record them by typing. PenPane can type them for you: open the settings window, turn drawing off in the toolbar (the settings window stays open), click the key box in the driver, then click **F13** under "Pen buttons" in the PenPane settings. The settings window never takes the keyboard focus, so the key lands in the driver's box. PenPane releases its own hotkeys for a moment so the key is not taken by its own binding. This does not work when the driver runs as administrator and PenPane does not. Otherwise assign a combination you can type, such as `Ctrl+Alt+Shift+F9`, and learn it with learn mode instead.
3. Leave the buttons in the driver's plain "keyboard key" mode, not a macro or an application shortcut that adds modifiers.
4. Start this application. The default bindings then give you: button 1 tap undo, button 1 double tap toggle overlay, button 2 tap next tool, button 2 double tap toggle mode, button 2 hold clear.

Pen buttons usually send a press and release in quick succession. Long gestures therefore depend on the driver reporting the key as held while the button is held; if a driver sends an instant press and release regardless, long gestures are not available for that button and you should use `single` and `double` there.

## Complete example

```
double_press_ms = 280
long_press_ms = 650
default_tool = select
default_stroke_index = 0
default_width_index = 1
auto_fade_seconds = 0
draw_monitor = 0
ui_monitor = 0
tool_lock = true
ui_auto_hide = true
ui_auto_hide_ms = 1200
ui_exclude_from_capture = true
toolbar_pos = 640,24
toolbar_pinned = true
properties_pinned = false
history_pinned = false
reset_bindings = true
bind = F13 single undo
bind = F13 double redo
bind = F14 single next_tool
bind = F14 long clear
bind = XButton1 single toggle_overlay
bind = Ctrl+Alt+F14 single copy_png
```
