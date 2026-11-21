# 0021. Excluding the tool islands from screen capture with WDA_EXCLUDEFROMCAPTURE

- Status: accepted
- Date: 2026-11-21

## Problem

When the user shares the screen or records a presentation, the drawing should appear but the tool islands should not.

## Decision

`SetWindowDisplayAffinity(window, WDA_EXCLUDEFROMCAPTURE)` (value `0x11`) is applied to all three island windows. `ui_set_capture_excluded` returns false when the call fails and does nothing else.

## Why

It is the only supported way to hide one window from capture APIs while keeping it visible on the physical display. It works with layered windows and costs nothing at run time.

## Alternatives rejected

- `WDA_MONITOR` (value `0x01`): the window shows as a black box in the capture instead of disappearing.
- Hiding the islands while a capture runs: there is no reliable signal that a capture started.

## Consequences

Requires Windows 10 version 2004 or later. On older systems the call fails, the islands stay visible in captures and the tray option can report that it is unsupported. The preference is applied at creation and whenever `ui_apply_settings` runs.
