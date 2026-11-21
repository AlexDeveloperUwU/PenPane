# 0015. A plain-text key/value configuration file

- Status: accepted
- Date: 2026-11-21

## Problem

Bindings, timings, defaults and panel positions must persist, be editable by hand, and never stop the application from starting.

## Decision

The settings live in one `key = value` text file. Loading starts from defaults, applies each valid line and skips and reports each invalid one. Values outside their documented range invalidate the line instead of being clamped. Saving writes a temporary file next to the target, flushes it and replaces the target in one step. A saved file begins its bindings with `reset_bindings = true` and lists the full table, so deleting a default binding survives a reload. Panel placements are written as `toolbar_pos = x,y` only once they have been set.

## Why

A line-oriented format is readable, diff-friendly and needs no parser library, which keeps the executable small. Skipping bad lines means a typo costs one setting, not the whole configuration. Rejecting rather than clamping makes mistakes visible in the report instead of silently running with a different value. The atomic replace means a crash or power loss while saving leaves either the old or the new file, never a half-written one. Without `reset_bindings`, an empty binding table would reload as the defaults, and a future change to the defaults would alter users' saved setups.

## Alternatives rejected

- JSON or INI through a library: extra code and size for a flat list of settings; INI sections add nothing here.
- Windows registry: not portable between machines, not editable with a text editor, harder to back up.
- Clamping out-of-range values: hides typos.
- Writing the file in place: a failure mid-write destroys the user's bindings.

## Consequences

Comments added by hand are lost when the application saves. Files larger than 256 KB are read only up to that size. The writer refuses to save, and reports failure, if the text would not fit its fixed output buffer, which the 64-binding limit makes unreachable.
