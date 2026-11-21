# 0010. RegisterHotKey instead of a global keyboard hook

- Status: accepted
- Date: 2026-11-21

## Problem

The application must react to pen-button keys (F13, F14 and similar) while any other program has the focus, and it must cost nothing while idle.

## Decision

Keyboard triggers are registered with `RegisterHotKey` on a hidden message window. `MOD_NOREPEAT` is not used. Auto-repeat suppression is done by the gesture engine, which ignores a press while the trigger is still marked as held.

## Why

A registered hotkey is delivered by the system only when the combination is pressed, so the process sleeps in `GetMessage` the rest of the time. A low-level keyboard hook sits in the path of every keystroke on the machine: a slow or blocked hook delays typing for the whole desktop, is disabled by Windows after a timeout, and is flagged by security products. `RegisterHotKey` also tells us when another program already owns a combination, which a hook cannot.

`MOD_NOREPEAT` would suppress repeats, but it is not enough for gestures. The engine needs to know when a key is held and when it is released, and it must see the first press of a double even when the key repeats. Keeping repeats visible and filtering them in one place keeps the behavior testable without Windows.

## Alternatives rejected

- Low-level keyboard hook always on: constant per-keystroke cost, risk of input lag, and it would let us swallow keys, which we do not need for normal operation.
- Raw Input in a message-only window: needs registration for the keyboard device class and delivers every key press to the process; no conflict detection and no way to claim a combination.
- `MOD_NOREPEAT`: cannot represent a held key for long gestures and hides the information the engine needs.

## Consequences

Another application can own a combination first; the failure is reported per trigger and the other triggers still work. RegisterHotKey reports presses only, so releases need another mechanism (see 0011). Only combinations the system can register are available, which excludes keys that Windows reserves such as `Win+L`.
