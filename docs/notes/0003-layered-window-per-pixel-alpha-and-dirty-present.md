# 0003. Layered window with a premultiplied DIB and dirty-rectangle presents

- Status: accepted
- Date: 2026-11-21

## Problem

Drawing needs to feel instant and cost almost nothing while idle, over arbitrary desktop content.

## Decision

One `WS_EX_LAYERED` window shows a 32-bit premultiplied DIB through `UpdateLayeredWindowIndirect`. Only the changed rectangle is composited by the OS (`prcDirty`). Redraw is event-driven: nothing runs unless input arrives or a trail is fading.

## Why

DWM composites the window on the GPU already, so the app needs no GPU code and no frame loop. Presenting only dirty rectangles keeps cost proportional to the stroke, not the screen.

## Alternatives rejected

- Direct2D or DirectComposition swap chain: larger working set, GPU dependency, extra DLLs.
- A render loop at 60 Hz: wastes CPU while idle, which is the main requirement.

## Consequences

Every present must pass position and size, otherwise some Windows builds skip the screen update. The surface must be released with the window to free memory.
