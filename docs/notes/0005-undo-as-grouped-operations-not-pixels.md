# 0005. Undo as grouped operations on editable elements

- Status: accepted
- Date: 2026-11-21

## Problem

The brief wants unlimited undo and fully editable drawings, not baked pixels.

## Decision

The scene is an ordered list of elements. Every change is an operation (add, remove, replace) inside a step; a step is what one user action did. Undo applies the inverse of a step, redo replays it. A new commit drops the redo stack.

## Why

Elements are small, so keeping every step is cheap, and grouped steps make erasing several elements or clearing the board one undo. Replacement operations store full before and after copies, which keeps resize, move and style edits trivial to invert.

## Alternatives rejected

- Pixel snapshots: tens of megabytes per step.
- Command objects per tool: more code for no benefit.

## Consequences

Memory grows with history; a pen stroke is stored twice only when it is later edited. History is session-only.
