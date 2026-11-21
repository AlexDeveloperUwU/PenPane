# 0006. Focus is taken only while drawing, and restored afterwards

- Status: accepted
- Date: 2026-11-21

## Problem

Shortcuts such as tool letters, Delete, Ctrl+Z and text typing need keyboard focus, but an overlay that steals focus breaks the presentation underneath.

## Decision

The overlay never activates in passthrough. When drawing starts it removes `WS_EX_NOACTIVATE` and takes the foreground, remembering the previous foreground window. When drawing ends it gives focus back.

## Why

Keyboard handling stays ordinary window messages, so no global keyboard hook is needed and nothing is swallowed system-wide. Focus changes only when the user asked to draw.

## Alternatives rejected

- Low-level keyboard hook while drawing: swallows keys system-wide and costs per keystroke.
- No keyboard shortcuts: violates the brief for tool letters and text.

## Consequences

Starting to draw moves the keyboard focus. Windows may refuse foreground changes, so a thread-input attach is used; if it fails, the toolbar and pen buttons still work.
