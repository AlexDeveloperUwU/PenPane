# 0004. Signed distance rasterizer with per-element maximum-coverage masks

- Status: accepted
- Date: 2026-11-21

## Problem

Strokes need clean anti-aliased edges, round caps and joins, variable width for pressure and no darker spots where segments overlap, especially with opacity below one.

## Decision

Each element is rasterized into an 8-bit coverage mask using distance fields (capsules for lines and pen segments, ellipse and rounded-rectangle distances for shapes). Overlapping parts of one element combine with a maximum, then the mask is blended once with the element color and opacity.

## Why

Round joins and caps come for free, pressure is just a radius that varies along a segment, and one blend per element removes double darkening. The code is small and has no external dependency.

## Alternatives rejected

- Scanline polygon fill of offset outlines: joins and tight turns need special cases.
- GDI+ or Direct2D: extra DLLs, larger memory, worse control of caps and pressure.

## Consequences

Cost per element is proportional to its bounding area times its segment count, which is fine for hand-drawn annotation. A shared scratch buffer avoids allocating per element.
