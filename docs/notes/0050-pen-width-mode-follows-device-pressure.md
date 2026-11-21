# 0050. Pen width follows real device pressure, otherwise stays constant

- Status: accepted
- Date: 2026-11-21

## Problem

Excalidraw's pen has a Pressure option. Its default is constant width. The "variable" mode simulates pressure from speed when no pen pressure exists. The brief asks to use pressure if present and degrade gracefully if not. The tablet in use behaves like an absolute mouse, so pressure may never arrive.

## Decision

The properties panel keeps the Pressure toggle (on by default). When it is on and the device reports pressure, the stroke width follows it. In every other case (toggle off, or no pressure from the device) the stroke has constant width, which is Excalidraw's default look. Speed simulation exists in `pen.c` (`PEN_PRESSURE_SIMULATED`) but is not used by the controller.

## Why

Clean, predictable strokes are what the brief asks for ("clean and smooth, not hand-drawn"). Speed-simulated width on a mouse-like tablet would make lines look inconsistent for no benefit to the user.

## Alternatives rejected

- Simulated pressure whenever the toggle is on: matches Excalidraw's variable mode on a mouse, but changes line width with hand speed.
- Pressure toggle off by default: forces users with a real pressure pen to turn it on.

## Consequences

On a device without pressure the toggle has no visible effect. If simulated variable width is wanted later, pass `PEN_PRESSURE_SIMULATED` in the controller; the code exists and is measured against Excalidraw (note 0040 and the Measured table in excalidraw-reference.md).
