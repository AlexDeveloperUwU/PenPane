# 0065. The tray icon is registered after the toolbar handle is visible

- Status: accepted
- Date: 2026-11-21

## Problem

Timed inside the process, start-up to a visible toolbar handle took about 28 ms and 14 to 18 ms of it was `tray_create`: the shell round trip of `Shell_NotifyIconW`. The handle is what the user looks for after launching.

## Decision

`app_run` loads the configuration and shows the toolbar handle first, then creates the tray icon and registers for session notifications. Notifications raised before the icon exists (configuration warnings, hotkeys that could not be registered) are queued in `App.pending` and delivered when the icon is created.

## Why

Nothing before the handle needs the icon. The only observable dependency was the early notifications, which the queue keeps.

## Alternatives rejected

- Creating the icon on a worker thread: adds a thread for a one-time 15 ms cost.
- Dropping the queue and losing early warnings: a behavior change.

## Consequences

Process start to first visible window, measured with `scripts\measure.ps1` (median of 15): 63.5 ms before, 47.5 ms after. The remaining time is process creation and the loader.
