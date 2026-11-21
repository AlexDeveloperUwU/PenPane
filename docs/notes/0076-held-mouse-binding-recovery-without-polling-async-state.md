# 0076. A stuck mouse binding is recovered by hook liveness and resume resets, not by polling button state

- Status: accepted
- Date: 2026-11-21

## Problem

A mouse button that is bound is swallowed by the low-level hook, and the gesture engine considers it held until it sees the release. If the hook is removed by the system between a press and its release, or if the machine sleeps or locks while the button is down, the release never arrives. The engine then ignores every later press of that button until restart.

## Decision

While a mouse trigger is held, the existing 12 ms timer checks two things. If the cursor moved but the hook has seen no event for 250 ms, the hook is dead. If the trigger has been held for 10 seconds, it is stale. In either case held mouse triggers are released and the hook is installed again. The same reset runs on resume from sleep, on unlock and on lock, and it also cancels learn mode.

## Why

Polling `GetAsyncKeyState` for the button, as is done for keyboard triggers, does not work here. A probe program installed a hook that swallowed an X button press and then read the state: the button read as up while it was physically down, because a swallowed event never updates the asynchronous key state. A poll would have reported a release 12 ms after every press and broken long-press gestures. A dead hook, on the other hand, is visible indirectly: moving the pointer must call the hook.

## Alternatives rejected

- Raw Input for button state: not verified to be independent of the hook, and adds an always-on registration.
- Not swallowing the press: the focused application would receive middle and side clicks.

## Consequences

A false positive (the cursor was moved by another program without hook events) releases a held gesture early, which is harmless. A button held deliberately for more than ten seconds is treated as released. The check costs nothing while no mouse trigger is held.
