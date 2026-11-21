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
