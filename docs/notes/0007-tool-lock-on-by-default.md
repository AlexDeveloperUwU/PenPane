# 0007. Tool lock is on by default

- Status: accepted
- Date: 2026-11-21

## Problem

Excalidraw returns to the selection tool after every shape unless the padlock is on. For annotating while presenting, switching to Selection after each pen stroke is disruptive.

## Decision

The padlock behavior exists exactly as in Excalidraw (Q toggles it) but the default configuration has it on (`tool_lock = true`).

## Why

The main use is drawing several strokes or arrows in a row.

## Alternatives rejected

- Excalidraw's default (off): more faithful, but worse for the intended use.

## Consequences

Anyone who prefers Excalidraw's behavior sets `tool_lock = false`.
