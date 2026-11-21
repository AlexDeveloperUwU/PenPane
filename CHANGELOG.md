# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

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

- Passthrough keeps a surface only as large as the annotations; memory is trimmed whenever the app is idle (hidden working set 10 MB to 0.3 MB).
- Rasterizer fast paths and a size-optimized build (99 KB release executable, faster startup).
- Text services are disabled when only plain Latin keyboard layouts are installed.

- Tool lock is on by default so the pen stays selected between strokes.

### Fixed

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
