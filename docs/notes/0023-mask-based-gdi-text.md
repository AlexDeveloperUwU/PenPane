# 0023. Text is rasterized by GDI into masks and blended by our canvas

- Status: accepted
- Date: 2026-11-21

## Problem

The islands and canvas text need good-looking text, but the surfaces are layered windows with per-pixel alpha, where GDI text drawing is unreliable and ClearType would color-fringe against a changing background.

## Decision

`text.c` draws text with GDI, white on black, into a small reusable 32-bit scratch DIB using `ANTIALIASED_QUALITY` (grayscale). The green channel becomes an 8-bit coverage `Mask` that the canvas blends with any color. Fonts are cached by role and size. The UI font is Segoe UI; the canvas font is Segoe Print when installed and Segoe UI otherwise. The scratch surface grows on demand and is released after unusually large requests.

## Why

Coverage masks are what every other shape in the program uses, so text gets the same blending, opacity and color handling and is correct on transparent pixels. Grayscale anti-aliasing has no color fringes, which matters when the background is the user's desktop.

## Alternatives rejected

- Drawing text directly into the layered surface with GDI: GDI ignores alpha and would leave transparent pixels.
- DirectWrite: better typography, but loads large components and the GPU stack that note 0001 avoids.
- Cleartype text: color fringes on a transparent background.

## Consequences

Each text draw allocates a mask. UI labels are few and static, so this is cheap. Text weight follows GDI's grayscale rendering and may look slightly lighter than ClearType.
