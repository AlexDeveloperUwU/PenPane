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

- Tool lock is on by default so the pen stays selected between strokes.

### Fixed

- Pen strokes now end at the release position.
- Undo, redo and clear no longer leave ghost selection boxes.
- The overlay is re-asserted as topmost when drawing ends.
- The test build defines `UNICODE` so every test links.
