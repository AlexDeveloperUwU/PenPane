# 0070. A hidden top-level popup owns timers, hotkeys and system notifications

- Status: accepted
- Date: 2026-11-21

## Problem

The window that owns hotkeys, timers, the tray icon and session notifications was created as a message-only window (`HWND_MESSAGE`). Message-only windows do not receive broadcast messages. `TaskbarCreated` (needed to put the tray icon back after Explorer restarts), `WM_DISPLAYCHANGE` and the legacy power broadcasts therefore never arrived, and the handlers for them were dead code.

## Decision

The window is a top-level `WS_POPUP` with `WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` that is never shown. Resume from sleep is received through `RegisterSuspendResumeNotification`, which delivers `WM_POWERBROADCAST` straight to the window. `app_apply_state` reads the monitor list again each time, so an unannounced layout change cannot leave the overlay on a monitor that no longer exists.

## Why

Broadcasts are sent to every top-level window, visible or not, so an invisible popup is enough and has no visual or taskbar effect. The explicit suspend and resume registration does not depend on broadcast delivery at all, which is the safer path for power events. Reading the monitors in `app_apply_state` costs one enumeration on a state change, never on a hot path.

## Alternatives rejected

- Keeping the message-only window and listening with a second, top-level window: two windows for one job and a second place for state.
- Polling the monitor layout with a timer: wakes an idle process for a rare event.

## Consequences

The window class is found by `FindWindow` from a second launch without special handling. The window is subject to session-end messages; the default handling is correct because the application keeps no state that needs saving on shutdown. Anything that must react to a broadcast can now rely on this window.
