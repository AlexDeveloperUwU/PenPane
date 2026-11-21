# 0080. Cancelling an interaction discards the tool-finished flag

- Status: accepted
- Date: 2026-11-21

## Problem

Committing text sets a flag that tells the application to return to the select tool when tool lock is off. Choosing another tool while text was being edited committed the text through the cancel path, which set the flag, but nobody consumed it. The next pointer event consumed the stale flag and switched the tool the user had just chosen to select.

## Decision

`controller_cancel` clears the flag at its end. The flag is set only by a deliberate finish, a stroke released or text committed by Esc or by clicking elsewhere, and is read by the pointer event that caused it.

## Why

A cancel is by definition not a completed gesture. Whatever it committed on the way, the tool change that caused it is the user's latest decision and must stand.

## Alternatives rejected

- Consuming the flag in `select_tool`: every other path that cancels would need the same line.

## Consequences

Committing text with a click still returns to select because that path does not go through cancel.
