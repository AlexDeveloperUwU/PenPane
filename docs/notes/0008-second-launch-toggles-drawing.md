# 0008. A second launch toggles drawing on the running instance

- Status: accepted
- Date: 2026-11-21

## Problem

The brief requires a single instance and says a second launch activates or closes the first.

## Decision

A named mutex detects the running instance; the second process posts a message to its hidden window and exits. The message toggles drawing mode.

## Why

It gives a launcher shortcut or script an easy way to start drawing without a hotkey, and never creates a second overlay.

## Alternatives rejected

- Closing the first instance: easy to trigger by accident during a presentation.

## Consequences

Exiting needs the tray, the toolbar button or the emergency hotkey.
