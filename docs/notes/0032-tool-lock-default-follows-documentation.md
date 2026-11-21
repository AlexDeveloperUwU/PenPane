# 0032. Tool lock is off by default

- Status: superseded by 0007 (default is true again, decided for the pen-annotation use case)
- Date: 2026-11-21

## Problem

`config_defaults` set `tool_lock` to true, while `docs/configuration.md` and `tests/test_config.c` both say false. The configuration test failed, and a saved configuration file recorded true.

## Decision

The code follows the documentation: the default is false, so the tool returns to select after one element, as in Excalidraw. Users who prefer to keep drawing with the same tool set `tool_lock = true` or press `Q`.

## Why

The documentation and the test agree and describe the shipped contract; the code was the outlier. Changing a documented default silently would also surprise anyone who read the documentation.

## Alternatives rejected

- Changing the documentation and test to true: friendlier for a pen overlay, but it is a product decision that belongs to the owner.

## Consequences

After one stroke the active tool becomes select and the properties panel hides until a tool or an element is chosen. If the owner prefers a locked pen, change the default in `config_defaults`, the table in `docs/configuration.md` and `test_defaults` together.
