# 0072. The emergency exit has a fallback chain and tells the user when it is degraded

- Status: accepted
- Date: 2026-11-21

## Problem

The emergency exit was registered with `RegisterHotKey` and the result was ignored. If another program already owned Ctrl+Alt+Shift+Esc, the documented way to always close the application silently did not exist, and nothing said so.

## Decision

Registration is tried in order: Ctrl+Alt+Shift+Esc with `MOD_NOREPEAT`, the same without the flag, then Ctrl+Alt+Shift+F12 with `MOD_NOREPEAT`. When the fallback combination is used, or when none can be registered, a tray notification says so and, in the last case, points to the tray menu.

## Why

An escape hatch that can fail silently is worse than none, because the user relies on it when input is trapped. The second attempt covers systems that reject the flag, and a different key covers a conflicting owner. Telling the user costs one balloon and removes the false assurance.

## Alternatives rejected

- A low-level keyboard hook for the combination: always-on cost and the risks described in note 0010.
- Registering a long list of fallbacks: the user cannot remember them, so one extra combination is the limit.

## Consequences

`docs/building.md` documents both combinations. The notification is queued until the tray icon exists, so it is shown even when the conflict is found at startup.
