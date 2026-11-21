# 0012. Learn mode installs hooks only for its duration

- Status: accepted
- Date: 2026-11-21

## Problem

Binding a pen button is easier when the user simply presses it, but that needs to see raw key and button events, including keys that are already registered as hotkeys, and to stop them reaching other programs.

## Decision

Learn mode installs a low-level keyboard hook and a low-level mouse hook when it starts and removes both as soon as it ends. It ends on a captured gesture, on `Esc` alone, on an explicit cancel, or after 15 seconds. The hook procedures only classify and record the event; the result is delivered to the owner by posting a timer message to the hidden window, so the owner's handler never runs inside a hook.

While active:

- Every key except modifiers is a candidate trigger and is swallowed. Modifier keys are swallowed and tracked by the module so they can be added to the captured trigger.
- Only the middle button and the two side buttons of the mouse are captured; left and right clicks pass so the interface stays usable.
- A key-up or button-up is swallowed only if its press was swallowed, so no key is left logically stuck.
- The gesture is classified like in normal use: hold at least the long-press time is long, two presses inside the double window is double, otherwise single (confirmed when the window passes). Auto-repeat presses are ignored.

## Why

A hook is the only way to observe and cancel a combination that is already registered as a hotkey. Limiting its life to a few seconds that the user started on purpose keeps the always-on cost at zero. Doing no real work inside the hook avoids the system timeout that silently removes slow hooks.

## Alternatives rejected

- Registering every possible key as a hotkey during learning: hundreds of registrations, conflicts, no mouse support.
- Keeping the hooks installed permanently and gating them with a flag: pays the per-event cost forever.
- Letting modifier keys through: an Alt press and release with nothing else between would activate a menu in the focused application.

## Consequences

During learning the user cannot use the keyboard in other applications. The emergency exit combination cannot act during learning because its modifiers are swallowed, so it cancels learning instead. `Esc` alone cannot be learned. Learn mode needs `SetWindowsHookEx` to succeed; if it fails the call returns false and nothing is left installed.
