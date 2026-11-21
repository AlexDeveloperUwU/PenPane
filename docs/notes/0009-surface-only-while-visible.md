# 0009. The drawing surface only exists while the overlay is visible

- Status: accepted
- Date: 2026-11-21

## Problem

A full-screen 32-bit surface is 8 MB at 1080p and 14 MB at 2560x1376, which dwarfs everything else in the process.

## Decision

The overlay window, its surface and the scratch mask are created when the overlay becomes visible and destroyed when it is hidden (drawing off with an empty desktop scene, or whiteboard off).

## Why

When nothing is on screen the process holds almost nothing. Scenes are element lists and survive without the surface.

## Alternatives rejected

- Keeping the surface for instant show: faster, but costs the largest memory block permanently.

## Consequences

Showing the overlay allocates and repaints once, which takes a few milliseconds.
