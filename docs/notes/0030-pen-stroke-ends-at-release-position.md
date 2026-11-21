# 0030. A pen stroke ends exactly where the pointer was released

- Status: accepted
- Date: 2026-11-21

## Problem

Streamlining moves the smoothed point only half of the way towards each raw sample, so the stored stroke always stops short of the last pointer position. On a fast flick the missing tail was about 14 px, visible as a stroke that ends before the pen does.

## Decision

`pen_finish` appends the last raw position as the final point when the stroke ends. The controller calls it when a pen draft is committed. It uses the position remembered by the builder, never the position of the release event, because the capture-lost paths deliver an event without a real position.

## Why

Streamlining is a smoothing aid while drawing; the end of a stroke is a point the user chose deliberately. Adding exactly one point keeps the shape smooth and costs nothing.

## Alternatives rejected

- Lowering the streamline factor: changes the feel of every stroke, and the constant belongs to the visual tuning work.
- Using the release sample: it can be a placeholder when capture is lost.

## Consequences

Strokes made of a single point (taps) are unchanged. Opaque strokes drawn directly while dragging get their last segment from the repaint of the committed element.
