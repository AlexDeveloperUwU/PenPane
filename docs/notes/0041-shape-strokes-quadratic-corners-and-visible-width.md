# 0041. Shape strokes: quadratic corners, 1.5 px thin width and exact dash periods

- Status: accepted
- Date: 2026-11-21

## Problem

Measured Excalidraw shapes differ from Ink's in three ways: rounded rectangle corners are not circular arcs, the thin preset is drawn 1.5 px wide instead of 1 px, and dashes follow `[8, 8 + width]` with round caps rather than Ink's pattern shifted by half a pixel.

## Decision

Rectangles are stroked and filled from one path whose corners are quadratic Beziers with the control point on the corner, as Excalidraw's rounded rectangle path is. The fill uses a new even-odd polygon rasterizer (`mask_add_polygon_fill`). Stroke widths below 1.5 are drawn at 1.5 for all shapes, while dash periods use the nominal width. Ellipse dashes are cut from a polyline whose step count follows the perimeter.

## Why

The corner dump at a 30 px radius differs from a circular arc by up to 1.3 px, enough to look different next to Excalidraw. Summed coverage across a width 1 line is 1.49 in Excalidraw, so 1 px lines would look visibly thinner. Dash run lengths now match pixel for pixel. The polygon fill keeps the fill edge under the stroke at the flatter corners; a circular fill would leave a sliver of background.

## Alternatives rejected

A signed distance function for the Bezier corner: no closed form, slower per pixel. Keeping the circular arc: visible corner difference. Rendering the thin preset at 1 px: a clearly lighter line than the reference.

## Consequences

The rectangle path has 53 points per draw and a full-screen filled rectangle costs about 6 ms. Hit testing in `hittest.c` still uses straight edges, which differ from the stroke by less than the hit tolerance.
