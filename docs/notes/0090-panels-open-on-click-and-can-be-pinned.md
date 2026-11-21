# 0090. Panels open on click and can be pinned

- Status: accepted
- Date: 2026-11-21

## Problem

Islands expanded as soon as the pointer touched their handle. Moving the pen across the screen opened panels by accident, and with drawing off the toolbar handle could not be opened at all because a click on it toggled drawing. There was also no way to keep a panel open while working.

## Decision

- Hovering a handle only highlights it. A press and release on the handle without dragging expands the island; dragging the handle still moves it. This works in every state, including drawing off.
- An expanded island that is not pinned collapses to its handle `ui_auto_hide_ms` after the pointer leaves it (default 1200 ms, one-shot timer). It never collapses while the pointer is inside it or while a button, slider or drag is active.
- Every island has a pin toggle right after its grip. A pinned island stays expanded whatever the pointer does and across drawing on and off while it is visible. The state is stored per island as `toolbar_pinned`, `properties_pinned` and `history_pinned`, and reported with `UI_EVENT_PANEL_PINNED` so the application saves it.
- With `ui_auto_hide = false` islands never collapse by themselves, as if all were pinned; handles still work.
- Drawing is started from the drawing button of the opened toolbar. A click on the handle no longer toggles it.

## Why

A click is a deliberate act and a hover is not, so opening on click removes accidental expansion and gives the handle a single meaning in every state. The delayed collapse keeps the islands out of the way without a second click, and the pin covers the cases where a panel should stay.

## Alternatives rejected

- Keeping expansion on hover with a pin: still opens by accident and keeps the drawing-off special case.
- Collapsing on a second click only: leaves panels covering the screen until the user remembers to close them.
- A global pin: panels differ in how long they are needed; the properties panel is often wanted while the history panel is not.

## Consequences

The timer rules of note 0022 still hold: no timer exists while an island is collapsed, pinned or under the pointer, so an idle process stays at zero CPU. A configuration reload applies pinned state at runtime. This replaces the hover expansion and the handle click that toggled drawing described in note 0022.
