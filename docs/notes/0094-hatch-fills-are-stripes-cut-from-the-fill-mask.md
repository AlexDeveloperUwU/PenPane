# 0094. Hatch fills are stripes cut from the fill mask

- Status: accepted
- Date: 2026-11-25

## Problem

Shapes only had a solid background. Users want Excalidraw-style hachure and cross-hatch fills. The rasterizer draws every shape into one shared scratch coverage mask and has no way to combine two masks.

## Decision

The shape's fill is rasterized as before. `mask_apply_hatch` in `src/hatch.c` then multiplies each pixel of that mask by the coverage of one or two families of diagonal lines, computed from the pixel's distance to the nearest line. Spacing and thickness follow the stroke width. The pattern is anchored at the shape's top-left corner. The default fill style stays solid.

## Why

Multiplying by an analytic stripe coverage needs no second mask, no intersect operation and no extra memory, and the edges of the line stay antialiased. Anchoring at the shape keeps the pattern fixed to the shape while it is dragged or resized by its corner, instead of shimmering across a canvas-anchored grid.

## Alternatives rejected

- Draw hatch lines as capsules into a second mask and intersect: needs a second scratch buffer and an intersect helper in the rasterizer, for the same picture.
- Draw the lines directly clipped to the outline: needs a clipper for each shape type.
- Rough, hand-drawn hachure like Excalidraw: random jitter would change on every repaint unless a seed is stored per element.

## Consequences

Hatch is only as accurate as one pixel; at very small sizes the stripes alias. Lines are always at 45 degrees. The cost is one pass over the shape's mask per repaint of a hatched shape.
