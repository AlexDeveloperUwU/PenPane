# 0024. Dirty updates pass position and size to UpdateLayeredWindowIndirect

- Status: accepted
- Date: 2026-11-21

## Problem

Hover feedback should repaint only the item that changed, which `UpdateLayeredWindowIndirect` supports through `prcDirty`.

## Decision

Every present, full or partial, goes through `UpdateLayeredWindowIndirect` with `pptDst` and `psize` filled in, and `prcDirty` set only for partial updates.

## Why

The documentation says that a null `pptDst` or `psize` leaves position and size unchanged, and the call reports success. In practice the window content did not refresh at all in that form on Windows 11 24H2: the call returned TRUE and the screen kept the old pixels, as verified by reading pixels back from the screen. Supplying the current geometry makes the update take effect.

## Alternatives rejected

- Null geometry with a dirty rectangle: silently does nothing.
- Always repainting and presenting the whole island: works, but burns more CPU on every pointer move.

## Consequences

The call sites stay simple: one function builds the update structure from the island state. If a future Windows version fixes the null form there is nothing to change.
