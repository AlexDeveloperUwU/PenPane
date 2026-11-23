# PenPane changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [0.5.0] - 2026-11-23

### Added

- Settings > General has two export switches, both on by default. "Export the full screen" exports the whole draw screen instead of only the painted area, in both modes. "Add desktop to exports" (desktop mode) puts the drawing over a screenshot of the desktop behind it, without the overlay or the tool panels. Config keys `export_full_screen` and `export_background`.

### Fixed

- Copying a drawing with the desktop background on no longer leaves keyboard focus on the window underneath, which sent the next paste to that window.

## [0.4.0] - 2026-11-23

### Added

- Installer, `PenPane-Setup-<version>.exe`, written in C: a wizard with welcome, license, folder and options, progress and finish pages. It installs per user to `%LOCALAPPDATA%\Programs\PenPane` without administrator rights, adds a Start menu shortcut and a Settings > Apps entry, and can add a desktop shortcut and start with Windows. `/S` installs silently, `/D=<folder>` picks the folder and `Uninstall.exe` removes everything.
- `scripts\package.ps1` builds the installer next to the portable `PenPane.exe` and writes a checksum for each.
- Releases now publish the installer and its checksum beside the portable executable.

### Changed

- PenPane quits when its window receives a close request, so the installer can replace a running copy cleanly.

## [0.3.0] - 2026-11-23

### Added

- Diamond tool (D) and an arrowhead choice (one head or two).
- Colour picker window with a brightness square, hue strip and hex field (type or paste), and a boxed picker button.
- Settings window with four tabs: General, Toolbar, Shortcuts and Pen keys.
- One extra shortcut per tool (Shortcuts tab): learn a key or pen button, or pick F13 to F18 with a click. The built-in keys stay.
- Pen keys tab: types F13 to F16 into the focused window, so a tablet driver's key box can record keys the keyboard does not have.
- Optional debug log (`debug_log`, off by default), capped at 1 MB and rotated to `PenPane.log.1`.
- "Reset all options" and "Reset menu positions" buttons.
- Release workflow that publishes a GitHub release for each version on `main`, and issue templates for documentation, hardware compatibility and questions.

### Changed

- The program, its configuration and its log are now `PenPane.exe`, `PenPane.ini` and `PenPane.log`.
- The app starts on the select tool.
- Tool lock now applies to every tool, pen, eraser and laser included, and is on by default.
- The desktop/whiteboard button moved to the bottom panel, and the settings gear moved to the end of the toolbar.
- Settings, Clear canvas and the colour picker are separate windows with rounded corners, a title band and more room.
- The eye button hides everything except itself and restores each panel to its previous open or closed state.
- Digit keys follow the toolbar order.
- New default colours: yellow added, red improved, softer pastel row.
- Short or quick laser strokes now fade tail first, like long ones.

### Fixed

- The laser and eraser fade animation froze while the pen or mouse kept moving, because its timer was reset on every redraw.
- Colour picker drags keep following the pointer outside the window.
- The 0.2.0 note "drawing memory about 16 MB to 2-5 MB" no longer holds: drawing mode now uses a full-monitor surface.

## [0.2.0] - 2026-11-22

- Renamed from Ink to PenPane (note 0091): `penpane.exe`, `penpane.ini`, window and synchronization object names, tray tooltip and registry value. An existing `ink.ini` next to the executable is copied to `penpane.ini` once (note 0092).

### Added

- Copy and Save buttons in the history panel, a Save drawing as PNG action (`save_png`) and a tray entry; the file dialog loads lazily.
- Text fonts: Handwritten, Normal (Segoe UI) and Legible (Atkinson Hyperlegible when installed, otherwise Verdana), chosen from a Font row in the properties panel.
- Laser Trace or Pointer option (`laser_pointer` in the configuration); pointer mode shows a dot that follows the pen or mouse.
- The toolbar starts expanded and folds away after the first hover, so the app is noticeable on dark backgrounds.
- Clearer drawing and interaction mode icons, placed first on the toolbar.
- Application icon and version information (`res/penpane.rc`, `res/penpane.ico`, `scripts/make-icon.ps1`); the tray tooltip shows the version.
- Public repository files: README, license (GPL-3.0-or-later, note 0093), contributing guide, code of conduct, security policy, issue and pull request templates, CI and release workflows, Doxyfile, `docs/README.md`.
- Panels open on click instead of hover, close shortly after the pointer leaves, and each has a pin button that keeps it open (saved as toolbar_pinned, properties_pinned, history_pinned).

- Transparent full-screen overlay with an anti-aliased software rasterizer and an alpha-1 backdrop that keeps drawing mode hit-testable while passthrough lets clicks reach the apps below.
- Desktop and whiteboard modes with separate scenes and unlimited undo and redo.
- Tools: selection (move, resize, delete), rectangle, ellipse, arrow, line, pen, laser, text and eraser, with Excalidraw's shortcuts and tool lock.
- Pen model measured against Excalidraw: constant width, or real device pressure when reported.
- Binding system: global hotkeys, single, double and long gestures, mouse buttons, learn mode and a plain-text configuration file.
- Floating toolbar, properties panel and history panel in Excalidraw dark style. They can be moved to another monitor, collapse after inactivity and are excluded from screen capture by default.
- Tray icon and menu: mode, drawing screen, tools screen, capture exclusion, remap buttons, copy as PNG, reload and open config, start with Windows (off by default).
- Single instance (a second launch toggles drawing), emergency exit hotkey, crash filter, display change, resume and unlock handling.
- Copy the drawing to the clipboard as PNG and bitmap.
- Documentation, decision notes and unit tests for the scene, controller, gestures and configuration.

### Changed

- Drawing mode keeps a visual surface sized to the annotations plus a constant-alpha input window (drawing memory about 16 MB to 2-5 MB); copying as PNG allocates only the annotation extent.
- The laser fade is linear instead of holding full width before vanishing.
- Passthrough keeps a surface only as large as the annotations; memory is trimmed whenever the app is idle (hidden working set 10 MB to 0.3 MB).
- Rasterizer fast paths and a size-optimized build (99 KB release executable, faster startup).
- Text services are disabled when only plain Latin keyboard layouts are installed.

- Tool lock is on by default so the pen stays selected between strokes.

### Fixed

- The eraser leaves no trace after the pen is lifted.
- Short laser strokes and taps now fade completely and reach the release position instead of stopping partway.
- Display changes, Explorer restarts and resume are now received (hidden top-level window); the tray icon is retried when registration fails.
- Emergency exit has a fallback chain and reports when it is degraded.
- Plain typing keys can no longer be bound as global hotkeys.
- An unreadable configuration file is never overwritten with defaults.
- Stuck mouse bindings are recovered on resume, unlock and lock; saves and PNG encoding no longer block the mouse hook.
- Slider drags are one undo step; clipboard export no longer clears the clipboard on failure; out-of-memory in the scene no longer leaves dangling elements.

- Pen strokes now end at the release position.
- Undo, redo and clear no longer leave ghost selection boxes.
- The overlay is re-asserted as topmost when drawing ends.
- The test build defines `UNICODE` so every test links.
