# Excalidraw dark-mode reference

Values extracted from the open-source Excalidraw repository (`master`). Only constants and visual language are reused, never its technology. Items marked **unverified** need a visual check against excalidraw.com in dark mode.

## Dark canvas

Excalidraw draws its canvas in light colors and applies the CSS filter `invert(93%) hue-rotate(180deg)` (`DARK_THEME_FILTER` in `common/src/constants.ts`). A matching JavaScript implementation exists as `applyDarkModeFilter` in `common/src/colors.ts`.

Per channel, in sRGB with no gamma:

1. Invert 93%: `v' = 0.07 * v + 0.93 * (255 - v)`
2. Hue rotate 180 degrees, then clamp and round:
   - `R'' = -0.574 R' + 1.430 G' + 0.144 B'`
   - `G'' = 0.426 R' + 0.430 G' + 0.144 B'`
   - `B'' = 0.426 R' + 1.430 G' - 0.856 B'`

Browser rounding can differ by one per channel.

| Light | Dark |
|---|---|
| `#ffffff` canvas | `#121212` |
| `#1e1e1e` default stroke | `#d3d3d3` |

## Palettes

Stroke picks: `#1e1e1e` `#e03131` `#2f9e44` `#1971c2` `#f08c00`.
Dark equivalents: `#d3d3d3` `#ff8383` `#3a994c` `#56a2e8` `#b76100`.

Background picks: transparent `#ffc9c9` `#b2f2bb` `#a5d8ff` `#ffec99`.
Dark equivalents: transparent `#5b2d2d` `#043b0c` `#154163` `#362600`.

Defaults: stroke `#1e1e1e`, background transparent, stroke width 2, opacity 100, solid style.

## Stroke

| Property | Value |
|---|---|
| Width presets | thin 1, medium 2 (default), bold 4 |
| Opacity slider | 0 to 100, step 10 |
| Dashed | `[8, 8 + width]` |
| Dotted | `[1.5, 6 + width]` |

## UI dark theme

| Role | Value |
|---|---|
| Island (toolbar, panel) background | `#232329` |
| Hover background | `#2e2d39` |
| Selected tool background | `#403e6a` |
| Selected tool icon | `#e0dfff` |
| Active border | `#d0ccff` |
| Text and icon | `#e3e3e8` |
| Keybinding hint | `#7a7a7a` |
| Input background | `#121212` |
| Input border | `#2e2e2e` |
| Primary | `#a8a5ff` |
| Island shadow | `0 0 1px rgba(0,0,0,.17), 0 0 3px rgba(0,0,0,.08), 0 7px 14px rgba(0,0,0,.05)` |

Sizes: island radius 8 px, button 36 px square with 16 px icons, island padding 4 px, toolbar divider 1 px by 24 px, keybinding hint 10 px at bottom-right inset 2 px and 3 px. Properties panel padding 12 px, max width 200 px, row gap 8 px.

## Tools and shortcuts

Selection `V`/`1`, rectangle `R`/`2`, ellipse `O`/`4`, arrow `A`/`5`, line `L`/`6`, pen `P` or `X`/`7`, text `T`/`8`, eraser `E`/`0`, laser `K`, tool lock `Q`. Diamond (`D`/`3`) and hand (`H`) are excluded by the project brief.

## Geometry and behavior

- Arrowhead (default style "arrow"): size 25 px, half angle 20 degrees for the full size. Tip is the line end; wings are the reversed direction vector rotated by plus and minus the half angle. The head size shrinks to at most half the last segment length. Source: `getArrowheadPoints` in `element/src/bounds.ts`.
- Pen: perfect-freehand with `size = width * 4.25`, `thinning 0.6`, `smoothing 0.5`, `streamline 0.5`, easing `sin(t * PI / 2)`. When pressure is not used it simulates pressure from speed. Output path is quadratic midpoint smoothing, filled.
- Eraser: trail with streamline 0.2, size 5, decay 200 ms. Elements touched by the trail are marked at 20 percent opacity and removed on release. Erasing restores when the trail crosses back with Alt.
- Laser: red trail, size 2, streamline 0.4, decay time 1000 ms, decay length 50, constant on-screen size.
- Text default: size 20, family Excalifont (not reproduced; a system font is used).

## Unverified

Exact icon path data, undo/redo placement, rounding versus browser, and whether the toolbar has a border. These are checked visually in the browser during the UI milestone.

## Measured

Measured on excalidraw.com in dark mode (a build whose pen has a "Pressure" option, canvas 1024 by 768 at device pixel ratio 1) by injecting elements with roughness 0 and summing pixel coverage of the canvas, then compared with PenPane's `tests/render_preview.c` output rendered from the same content. PenPane widths 1, 2 and 4 correspond to Excalidraw's pen widths thin 0.5, medium 1 and bold 2, because the pen presets are half the shape presets. Thickness is the integral of coverage across the stroke in pixels.

| Property | Excalidraw | PenPane |
|---|---|---|
| Pen default mode | constant width (the other mode is "variable") | constant via `PEN_PRESSURE_CONSTANT`; speed simulated via `PEN_PRESSURE_SIMULATED` |
| Pen constant thickness for sw 1 / 2 / 4 | 2.75 / 5.49 / 11.24 | 2.74 (width 2) / 5.48 (width 4) / 10.98 (width 8) |
| Pen variable thickness, sw 1, event spacing 1 / 2 / 3 / 4 / 6 / 8 / 12 px | 7.24 / 6.25 / 4.75 / 3.01 / 2.50 / 2.50 / 2.50 | 7.31 / 6.17 / 4.74 / 3.06 / 2.62 / 2.62 / 2.62 (width 2) |
| Pen variable thickness, sw 2, same spacings | 15.49 / 14.50 / 13.49 / 12.25 / 9.49 / 6.25 / 5.24 | 15.48 / 14.61 / 13.56 / 12.35 / 9.47 / 6.13 / 5.25 (width 4) |
| Pen variable thickness, sw 4, same spacings | 31.74 / 31.01 / 30.00 / 29.24 / 27.01 / 24.25 / 19.01 | 31.67 / 30.93 / 30.11 / 29.20 / 27.11 / 24.67 / 18.95 (width 8) |
| Pen outline diameter | 2 x size x sin(t x PI / 2), size = 4.25 x sw, t = 0.5 - 0.6 x (0.5 - pressure) | width x 4.25 x sin(...) with width = 2 x sw |
| Pen streamline factor | 0.15 + (1 - 0.5) x 0.85 = 0.575 per sample | same |
| Pen speed unit | distance between streamlined points divided by 4.25 x sw | same, 2.125 x width |
| Shape stroke thickness for width 1 / 2 / 4 | 1.49 / 2.00 / 4.00 | 1.5 / 2.0 / 4.0 |
| Dash runs for width 1 / 2 / 4 (lit, unlit) | +9 -8 / +10 -8 / +12 -8, period 8 + 8 + width | identical runs |
| Dotted period for width 2 / 4 | 9.5 / 11.5 (dot 1.5 plus caps) | same |
| Dotted arrowhead | dotted | solid |
| Rounded rectangle corner | quadratic Bezier with the control point on the corner, radius min(side) x 0.25 capped at 32 | same curve (12 steps per corner), polygon fill of the same path |
| Arrowhead, long arrow | wings 25 px long at 20 degrees, round caps, independent of stroke width | same |
| Arrowhead, 30 px arrow | wing about 15 px at about 28 degrees | wing 15 px at 26.7 degrees |
| Selection outline | 1 px, `#b3afff`, square corners | same, box snapped to whole pixels |
| Selection handles | 8 px rounded square (radius about 2), 1 px outline `#b3afff`, fill `#121212`; only four corner handles plus a rotation circle seen on the ellipse measured | four corner handles, same look, no rotation circle |
| Selection padding | handle centres 4.5 px outside the geometry for width 2 | 3.5 + width / 2 |
| Laser trail | red fill, head radius 2, no glow, 1000 ms decay, taper over the last 50 points, streamline 0.4 | same |
| Eraser trail | fill `rgba(255,255,255,0.2)`, head radius 5, 200 ms decay, taper over 10 points, streamline 0.2 | same |
| Anti-aliasing | exact area coverage blended in sRGB without gamma (a 2.75 px line gives edge pixels 0.37) | distance based coverage blended without gamma, edge values within 0.03 |

Notes:

- Excalidraw's default roughness is 1 (hand drawn). All comparison shapes use roughness 0 so that edges are clean.
- Excalidraw's default pen stroke width is 1, which yields the 2.75 px constant stroke; PenPane's default width 2 is the same preset by name.
- Controller values still to align (outside this task's files): `LASER_RADIUS` 2, `ERASER_RADIUS` 5, `ERASER_COLOR` `COLOR_ERASER_TRAIL`, pen mode argument `style.pressure ? sample->pressure : PEN_PRESSURE_CONSTANT`, and a call to `pen_finish` on pointer release. The trail constants are exported from `src/trail.h` as `TRAIL_LASER_RADIUS`, `TRAIL_ERASER_RADIUS`, `TRAIL_LASER_LIFETIME_MS` and `TRAIL_ERASER_LIFETIME_MS`.
- Re-run the comparison with `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\preview.ps1`, which compiles `tests\render_preview.c` with the render sources, writes PNG files to `build\preview` and prints the same tables and pixel dumps.
