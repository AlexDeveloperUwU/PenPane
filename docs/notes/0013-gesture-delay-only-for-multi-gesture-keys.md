# 0013. Gesture delay only for keys that need it

- Status: accepted
- Date: 2026-11-21

## Problem

Supporting double presses means a single press cannot be acted on until we know no second press follows. Applying that wait to every key would make every pen button feel sluggish.

## Decision

The gesture engine decides per trigger from its bindings:

- Single only: the action fires on press, with no delay.
- Single and long: the long action fires at the long-press threshold while the key is held; a shorter press fires the single on release.
- Any trigger with a double binding: the single action fires only when the double window has passed since the release.

The engine is platform independent. It receives press and release events with a millisecond clock, never reads time or starts timers itself, and returns the time until its next deadline, or -1 when nothing is pending. Resolved actions are queued by the hotkey module and delivered after the engine call returns.

## Why

The delay is a cost the user pays only on triggers where they asked for double gestures, and the documentation says so. A clock-free engine can be tested exhaustively with plain asserts, including clock wrap-around and late events, and the `-1` return lets the caller kill its timer so the process does not wake while idle. Delivering actions after the call prevents a handler that reconfigures the bindings from changing the engine while it is iterating.

## Alternatives rejected

- A fixed delay on every key: simple, but taxes triggers that do not need it.
- Firing the single immediately and the double in addition: the user would get both actions on a double press.
- An engine that owns its timers: ties it to Windows and makes timing tests slow and flaky.

## Consequences

Users must choose between instant singles and doubles on the same key. A late event is handled correctly because every call first advances any deadline that has already passed. The engine tracks at most 16 triggers and 64 bindings; extra triggers are ignored.
