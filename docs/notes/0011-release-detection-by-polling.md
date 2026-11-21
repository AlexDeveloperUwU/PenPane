# 0011. Release detection by polling while a key is held

- Status: accepted
- Date: 2026-11-21

## Problem

`RegisterHotKey` reports a press but never a release, yet long and double gestures and auto-repeat suppression all depend on knowing when the key goes up.

## Decision

While at least one keyboard trigger is held according to the engine, a timer polls `GetAsyncKeyState` for the held keys every 12 ms and reports their releases to the engine. When nothing is held the poll stops; the same timer is then used only to wake up for the next gesture deadline, and it is killed when the engine has nothing pending.

## Why

A tap lasts around 100 ms, so a poll costs about eight cheap calls per press and nothing between presses. The 12 ms period is below the resolution that matters for gestures (hundreds of milliseconds) and close to the system timer granularity. A single timer id serves both purposes: when keys are held it ticks the engine every poll, when none is held it sleeps until the next deadline.

The poll is run for every bound keyboard trigger, not only those with long or double bindings, because the engine treats a trigger as held until it sees a release. That is what makes auto-repeat harmless for single-only triggers too.

## Alternatives rejected

- A permanent low-level keyboard hook to see key-ups: always-on cost, see 0010.
- Polling continuously: wastes CPU while idle and defeats the purpose of the tool.
- Assuming a fixed press length: breaks long gestures and holds.

## Consequences

Release time has up to about 12 ms of jitter, plus the system timer granularity of roughly 15 ms. A key released and pressed again between two polls looks like one long press; that cannot happen at human speed. A held state can only persist until the next poll sees the key up, which is bounded by the poll period.
