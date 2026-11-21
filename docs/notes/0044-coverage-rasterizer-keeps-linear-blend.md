# 0044. Rasterizer keeps linear coverage blending and limits capsule work per row

- Status: accepted
- Date: 2026-11-21

## Problem

Edge quality needed a check against the browser (gamma, softness), and long diagonal strokes cost time proportional to their bounding box.

## Decision

Keep signed distance coverage (`0.5 - d`) blended in sRGB without gamma. Restrict each capsule to the pixel span it can touch on every row.

## Why

Excalidraw's canvas produces edge pixels equal to exact area coverage with no gamma: a 2.75 px line centred on a pixel boundary has edge values 0.37, which Ink reproduces within 0.03. Adding gamma would make Ink's strokes look lighter than the reference. A 1200 px diagonal arrow touched about one million pixels per capsule; the row spans cut a diagonal arrow to well under a millisecond.

## Alternatives rejected

Gamma correct blending: differs from the browser. Supersampling: three to four times the cost for no visible gain.

## Consequences

Strokes match Excalidraw's softness. Overlapping segments of one element still merge with a maximum, so translucent strokes never darken twice.
