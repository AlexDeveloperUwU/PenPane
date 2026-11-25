# 0095. Line and arrow are one tool

- Status: accepted
- Date: 2026-11-25

## Problem

The toolbar had separate arrow and line tools that drew the same segment. The only difference was the arrowhead, which was already a style option for arrows (one head or two). Two buttons, two icons and two shortcuts for one shape made the toolbar longer than it needed to be.

## Decision

There is one tool, `line`, with shortcut `L` and the arrow icon. A line has an arrowhead style: none (the default), one head at the end, or heads at both ends, chosen in the properties panel. The `arrow` tool, its `A` shortcut, the `ELEMENT_ARROW` element type and the `tool_arrow` action are removed.

The names the old versions wrote are still read: `default_tool = arrow` and `tool_arrow` bindings mean the line tool, and `arrow` is ignored inside `tool_order` and `hidden_tools` lists, because the line entry that always sits next to it carries the position and the visibility.

## Why

One shape with a style option is simpler to use and to maintain than two tools that must stay in sync. Ignoring `arrow` in the lists keeps every configuration that the program itself wrote loadable: it always listed both names, so rejecting the duplicate would have thrown away the whole line and reset the user's toolbar order.

## Alternatives rejected

- Keep `A` as a second shortcut for the line tool with arrowheads preselected: it hides a mode behind a key, and the properties panel already shows the choice.
- Refuse the old names: it would reset the toolbar order or hidden tools of every existing user once.
- Default the merged tool to one end head: most lines are plain and an arrowhead is one click away; a plain line also matches what the line tool did before.

## Consequences

An element's arrowheads are part of its style, so they can be changed after drawing and are undoable like any other style change. Someone who hid only `arrow` now sees the line tool. The default style of new lines has no head, so people who used `A` need one extra click on the panel.
