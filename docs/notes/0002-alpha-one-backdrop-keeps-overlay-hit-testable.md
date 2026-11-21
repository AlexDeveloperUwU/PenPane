# 0002. Alpha-1 backdrop keeps the overlay hit-testable

- Status: accepted
- Date: 2026-11-21

## Problem

In drawing mode the overlay must receive every pen event, but it is a transparent layered window. Windows treats pixels with alpha 0 in a per-pixel-alpha layered window as click-through even when `WS_EX_TRANSPARENT` is removed, so an empty overlay would let strokes land on the application below.

## Decision

While drawing, the backdrop is filled with alpha 1 (invisible, roughly 0.4 percent). While not drawing it is alpha 0 and the window also has `WS_EX_TRANSPARENT`.

## Why

It is the cheapest way to make the whole screen reliably capture input without a second window or a hook. The cost is one extra full-surface repaint when switching state.

## Alternatives rejected

- Low-level mouse hook that forwards or swallows input: runs on every mouse event system-wide and is fragile.
- Color-key layered window: no anti-aliased edges over the desktop.
- A nearly transparent opaque child window: extra window, extra memory, same visual result.

## Consequences

State switches repaint the whole surface. The behaviour was verified with synthetic input (see input-model.md).
