# 0042. Laser and eraser trails: no glow, width taper by age and point distance

- Status: accepted
- Date: 2026-11-21

## Problem

PenPane's laser had a translucent halo and a quadratic width fade, and its radii (4 and 3) were guesses. Excalidraw's trails keep one fixed colour and shrink instead of fading.

## Decision

Draw one solid pass per trail. The width of each point is the maximum radius times the smaller of two quartic ease-outs: one of the remaining life (1 - age / lifetime) and one of the distance to the newest point, measured in 16 ms frames, over 50 frames for the laser and 10 for the eraser. Positions are streamlined with factors 0.66 (laser) and 0.83 (eraser). Excalidraw's radii are 2 and 5, lifetimes 1000 ms and 200 ms, and the eraser colour is white at 20 percent alpha; all are exported from `trail.h`.

## Why

DOM inspection of the live trails showed a red `fill` path with a 4 px head for the laser and `rgba(255, 255, 255, 0.2)` with a 10 px head for the eraser, and no second path for a glow. Measuring the taper in frames rather than pixels follows Excalidraw's per-event point counting while staying independent of the pointer rate (a 1000 Hz mouse would otherwise collapse the taper to 50 ms). The quartic ease-out is taken from the source and matches the near uniform body with a short collapsing tail seen on screen; it was not isolated numerically.

## Alternatives rejected

Keeping the glow: not present in the reference. Pixel based taper: Excalidraw's trail stays uniform when few points exist, which a pixel taper would not reproduce.

## Consequences

`controller.c` still passes radius 4 and 3; changing them to the exported constants completes the match. The trail now draws a single mask, so it is cheaper than before.
