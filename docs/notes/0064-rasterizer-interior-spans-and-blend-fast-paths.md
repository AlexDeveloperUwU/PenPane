# 0064. Rasterizer fast paths that produce identical pixels

- Status: accepted
- Date: 2026-11-21

## Problem

A large ellipse or rectangle evaluated its distance function, with a square root, for every pixel of its bounding box although the interior contributes nothing to a stroke and a constant to a fill. The distance function was called through a pointer for every pixel, and the blend loop did four divisions for pixels that are fully covered.

## Decision

- `rasterize_inline` is always inlined into three wrappers (`rasterize_capsule`, `rasterize_ellipse`, `rasterize_rect`), so each shape gets its own loop without calling through a pointer.
- Ellipse and rectangle rows ask a span function for the pixels that are certainly deep inside (a guaranteed bound for the ellipse distance, an exact one for the rectangle, plus a margin of 0.02 pixel). Strokes skip them, fills write 255.
- `canvas_blend_coverage` writes a solid pixel directly for full coverage with an opaque color, skips eight zero coverage bytes at a time and clips the row once.

## Why

The skipped pixels would have produced exactly the values written or skipped now: zero coverage for a stroke, full coverage for a fill. Output is verified, not assumed: 4000 random shapes (fills, strokes up to 30 px, shadows, corner radii up to 60 px, partly outside the mask) compared against the previous rasterizer give 0 differing pixels, and `scripts\preview.ps1` output is byte for byte identical for all seven images.

## Alternatives rejected

- Skipping the exterior of an ellipse: the distance is an approximation without a usable bound there.
- Avoiding the square root by comparing squared distances at the band edges: changes values by rounding in the last bit at the boundary, which breaks byte equality.
- SIMD: not needed once the interior is skipped.

## Consequences

Measured in the benchmark build (`-Os`): full-screen ellipse stroke 19.6 to 5.3 ms, filled rectangle with outline 15.1 to 7.9 ms, long pen stroke full repaint 20.1 to 15.4 ms, 200-stroke full scene 67 to 51 ms. Per pen event the rasterizer costs about 24 microseconds, so the pointer path is dominated by the present call (note 0003).
