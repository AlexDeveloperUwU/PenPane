# 0061. Idle moments give memory back: working set, mask scratch buffer

- Status: accepted
- Date: 2026-11-21

## Problem

After startup, after drawing is switched off and after the laser has faded, the process sat on pages it would not touch again for a long time: start-up code of every loaded library, the last surface contents, and a mask scratch buffer sized for the largest element rendered so far (3.5 MB for a full-screen shape).

## Decision

- `trim_working_set` calls `SetProcessWorkingSetSize(-1, -1)`. It runs once before the message loop, at the end of every state change that leaves drawing off, and when the trail animation timer stops.
- `mask_scratch_trim` frees the scratch buffer at the end of a flush when it grew beyond 256 KB. The buffer is allocated again on the next large element.
- Neither is called from the pointer path.

## Why

Trimmed pages go to the standby list and come back as soft faults only if they are touched. Idle processes touch almost nothing, so the reported working set stays low. A trim at the moment of going idle costs well under a millisecond and happens once per transition, never per event.

## Alternatives rejected

- Trimming on a timer: a periodic wake-up is exactly what the rest of the design avoids.
- Trimming while drawing: the first stroke after a trim faults in the pages it touches, which shows up as latency exactly when the user is active.
- Shrinking the island canvases: they are already freed when an island hides (note 0022) and a collapsed handle canvas is a few kilobytes.

## Consequences

Measured working set: hidden 10.0 to 0.3 MB, drawing after the laser faded 29.7 to 2.8 MB, drawing off 16.3 to 1.6 MB. Private bytes (committed memory) are not changed by a trim; only the surface and scratch release lower them. The working set rises again while drawing because the surface is rewritten.
