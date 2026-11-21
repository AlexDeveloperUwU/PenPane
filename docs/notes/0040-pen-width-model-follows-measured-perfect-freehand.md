# 0040. Pen width model follows measured perfect-freehand

- Status: accepted
- Date: 2026-11-21

## Problem

The pen formula in `src/pen.c` was a guess: diameter equal to `size x sin(...)` with `size = width x 4.25`, and speed measured in raw pointer distance. Nothing confirmed it against Excalidraw, and current Excalidraw has a "Pressure" option (constant or variable) that the notes did not cover.

## Decision

Keep the diameter formula `width x 4.25 x sin(t x PI / 2)` with `t = 0.5 - 0.6 x (0.5 - pressure)`. It differs from the literal perfect-freehand radius `size x easing(...)` only by the factor two between Ink's width presets (1, 2, 4) and Excalidraw's pen widths (0.5, 1, 2). Fix the speed model: streamlining uses perfect-freehand's factor 0.575 and speed is the distance between streamlined points over `2.125 x width`. Add a constant width mode (`PEN_PRESSURE_CONSTANT`, stored pressure 0.0158, diameter 1.37 x width) and `pen_finish` to place the last point exactly under the cursor.

## Why

Pixel measurements of Excalidraw strokes at event spacings from 1 to 12 px reproduce the formula within 0.12 px for all three widths (see the Measured section of `docs/excalidraw-reference.md`). Excalidraw's pen widths are half the shape widths, so the literal formula with Ink's presets would draw strokes twice as thick; folding the factor into the size keeps the constants recognizable. Excalidraw's default is the constant mode, so it must exist. The final point matters because streamlining lags the pointer by about 40 percent of the last step, which is visible on fast strokes.

## Alternatives rejected

Using the literal formula with Ink's presets: doubles every stroke. Re-mapping the presets to 0.5, 1 and 2: changes the width presets, the style model and the UI for no visual gain. Making constant width the only mode: loses the speed dependent look the project brief asks for.

## Consequences

`controller.c` must pass `PEN_PRESSURE_CONSTANT` when the pressure option is off and call `pen_finish` on release to get the full effect; until then the pen behaves as the variable mode. Hit testing keeps `pen_radius(width, 0.7)`.
