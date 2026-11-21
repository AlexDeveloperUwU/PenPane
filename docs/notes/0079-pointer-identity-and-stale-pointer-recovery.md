# 0079. Only the pointer that started a stroke may continue or end it

- Status: accepted
- Date: 2026-11-21

## Problem

Pen, touch and mouse messages all fed one state flag. A second finger, or a pen hovering while a finger drew, produced updates without contact that were read as the end of the stroke, and a second press while one was running restarted it.

## Decision

The overlay remembers the identifier of the pointer that started the stroke. Updates and releases from other pointers are ignored, and so are presses from other pointers while a stroke runs. If the active pointer is no longer in contact (its release was lost), a new press ends the old stroke first and then proceeds. Mouse presses follow the same rule using the left button state.

## Why

Ignoring foreign input keeps one stroke consistent. The liveness check prevents the opposite failure: a lost release must never lock out every later press.

## Alternatives rejected

- Ignoring foreign presses unconditionally: a missed release would block drawing until restart.
- Supporting several simultaneous strokes: the controller has one draft element.

## Consequences

Multi-touch drawing is not supported. Palm touches during a pen stroke no longer interrupt it.
