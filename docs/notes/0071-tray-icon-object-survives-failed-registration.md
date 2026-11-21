# 0071. The tray icon object survives a failed registration and is retried

- Status: accepted
- Date: 2026-11-21

## Problem

`tray_create` destroyed itself and returned NULL when the shell refused the icon. This happens at logon when the application starts before the taskbar exists, and sometimes when Explorer is busy. The application then had no tray object at all: no icon until restart, `TaskbarCreated` was ignored because it checked for the object, and notifications such as configuration warnings were lost.

## Decision

`tray_create` returns the object even when the shell refuses the icon, and records whether the icon is registered. The application retries with a one-shot timer that starts at two seconds and doubles up to 30 seconds, and also retries on `TaskbarCreated` and after a resume. A failed `NIM_ADD` is followed by `NIM_MODIFY`, because the shell can time out an add that actually succeeded. Notifications wait in a short queue until the icon is registered, and the oldest is dropped when the queue is full.

## Why

The icon is the only way to quit or configure the application, so a transient shell problem must not become permanent. A one-shot timer that re-arms itself costs nothing once the icon exists, and the backoff keeps a long outage cheap.

## Alternatives rejected

- A periodic timer that runs for the whole session: wakes an idle process forever.
- Exiting when the icon cannot be created: the user would lose the application over a cosmetic failure.

## Consequences

`tray_notify` now reports whether the shell accepted the balloon so the caller can keep the message. While the icon is missing, the emergency hotkey and the second-launch toggle remain the ways to reach the application.
