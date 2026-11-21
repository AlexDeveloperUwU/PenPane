# 0068. Rejected: a surface-free capture layer, a custom heap, other micro-savings

- Status: accepted
- Date: 2026-11-21

## Problem

In drawing mode the 14.1 MB surface is 90 percent of private bytes (15.3 of 15.8 MB).

## Decision

Keep one full-monitor surface while drawing.

## Why

Input capture needs every pixel of the window hit-testable. A per-pixel-alpha layered window passes clicks through pixels of alpha 0, so the surface must hold alpha 1 everywhere (note 0002). A constant-alpha layered window (`SetLayeredWindowAttributes`) is hit-testable everywhere without a bitmap in this process, and a second, small per-pixel window could present the strokes. That design needs a content window that grows with the strokes (each growth copies pixels), correct stacking of two topmost windows, input routing across both, an opaque whiteboard variant, and behavior of an unpainted constant-alpha window under DWM that could not be verified. The saving would be about 13 MB while drawing, which is real, but the risk touches the core of the input model and was not taken in this pass.

## Alternatives rejected

- Custom heap (`HeapCreate`): the hidden process commits 1.6 MB in total and the surface is allocated by GDI, not the heap; there is no fragmentation to remove.
- Sixteen-bit or paletted surfaces: `UpdateLayeredWindow` needs 32 bits per pixel.
- Smaller scratch mask for text: text masks are freed after each render already, and the text surface is released above 4 MB (note 0023).
- `EmptyWorkingSet` while drawing: see note 0061.

## Consequences

Drawing keeps a 14 MB committed surface on a 2560x1376 monitor. The two-window design is the largest remaining memory idea.
