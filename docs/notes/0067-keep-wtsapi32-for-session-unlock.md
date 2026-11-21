# 0067. Session notifications stay on wtsapi32

- Status: accepted
- Date: 2026-11-21

## Problem

`wtsapi32` pulls `winsta` and two more modules into the process only to learn that the session was unlocked, so the overlay can be re-presented.

## Decision

Keep `WTSRegisterSessionNotification`; register it after the toolbar handle is visible (note 0065) so it no longer delays start-up.

## Why

Without it the hidden process has 4 modules fewer (22 instead of 26 at the time of the test) and 0.14 MB fewer private bytes (1.62 to 1.48 MB; drawing 15.86 to 15.72 MB). The replacement candidates cannot be verified in the test environment, where the session cannot be locked and unlocked: a window event hook on desktop switches fires on lock and unlock but cannot tell them apart, and power-setting notifications do not cover unlock. A lost re-present after unlock leaves annotations buried or stale, which is a visible bug, against 0.14 MB.

## Alternatives rejected

- Loading `wtsapi32` with `LoadLibrary` at start: same modules in memory.
- `SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH)`: unverifiable here.

## Consequences

The hidden process keeps `wtsapi32` and `winsta` in its module list.
