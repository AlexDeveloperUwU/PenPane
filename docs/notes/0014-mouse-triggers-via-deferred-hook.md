# 0014. Mouse triggers through a conditional low-level hook with deferred handling

- Status: accepted
- Date: 2026-11-21

## Problem

Mouse side buttons are not keyboard keys and `RegisterHotKey` cannot see them. The only supported way to see and consume them globally is a low-level mouse hook, which is dangerous if it does real work or exists when not needed.

## Decision

A `WH_MOUSE_LL` hook is installed only when at least one binding uses `MButton`, `XButton1` or `XButton2`, and removed when none does (and while learn mode does not need it). The hook procedure does the minimum: it recognizes the three buttons, checks whether the trigger is bound, and for bound buttons records the event in a small queue, posts a timer message to the hidden window and returns "swallowed". Everything else, including every move event and all other buttons, is passed on immediately. A button-up is swallowed only if its button-down was swallowed. The gesture engine and the owner's action handler run later, from the window procedure.

## Why

Windows removes a low-level hook that blocks for too long, and a slow hook lags the cursor of the whole machine. Posting the event moves all work out of the hook. Swallowing only bound buttons keeps the other buttons and the pointer behaving normally. Because the queue is touched only on the thread that owns the window, no locking is needed.

## Alternatives rejected

- Running the engine and the handler inside the hook: risks the hook timeout whenever a handler is slow.
- Raw Input for mouse buttons: sees the buttons but cannot consume them, so the focused application would still receive the click.
- Keeping the hook always installed: wasted per-event cost for users who bind only keys.

## Consequences

A bound middle or side button no longer reaches other programs. Event handling is one message later than the hook, which is far below the gesture timings. At most 32 events are queued; further events are dropped, which cannot happen at human speed. Only one module instance may exist because the hooks are process wide.
