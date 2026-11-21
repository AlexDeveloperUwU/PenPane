# 0077. A missing monitor is handled by a fallback, not by rewriting the configuration

- Status: accepted
- Date: 2026-11-21

## Problem

When the configured drawing or panel monitor was not connected, the monitor refresh replaced the stored index with 0. The next save, caused by any unrelated change, made that permanent: after reconnecting a projector, the application no longer used it.

## Decision

The configured indexes are never modified by the monitor logic. `draw_monitor` and `panel_monitor` return the configured index when such a monitor exists and 0 otherwise, and every use of an index goes through them. Only an explicit choice in the tray menu changes the stored value.

## Why

The index describes what the user wants; whether it can be satisfied right now is a runtime fact. Keeping them separate lets a monitor that comes back be used again without any action.

## Alternatives rejected

- Clamping on load and on every display change: the loss described above.
- Storing monitors by device name: more robust but a larger change than this problem needs.

## Consequences

While a monitor is missing, the tray menu shows the fallback monitor as selected, and choosing a screen stores that choice.
