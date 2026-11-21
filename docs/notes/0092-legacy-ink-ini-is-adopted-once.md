# 0092. A legacy ink.ini is adopted once

- Status: accepted
- Date: 2026-11-21

## Problem

The rename (note 0091) changes the configuration file from `ink.ini` to `penpane.ini`. Someone who ran an earlier build has bindings, positions and preferences in `ink.ini`; starting the renamed program would silently reset them to the defaults.

## Decision

At startup, before the configuration is loaded, `config_migrate_legacy` runs. When `penpane.ini` does not exist (the file is absent, not merely unreadable) and an `ink.ini` exists next to the executable, the legacy file is copied to `penpane.ini`. The normal load then reads the copy, and later saves go to `penpane.ini`. The legacy file is left in place and never read again, because `penpane.ini` now exists. If the copy fails, the defaults are used as for any missing file.

## Why

A copy keeps the user's text byte for byte, including keys a newer version may add, and can be repeated by hand by deleting `penpane.ini`. Leaving the old file protects the user's data if they go back to an older build. Treating "exists but unreadable" as not missing follows note 0074: only a file that is really absent may be replaced.

## Alternatives rejected

- Load the legacy file and save it under the new name: rewrites the user's file through the parser and drops what it does not understand.
- Move or delete the legacy file: destroys data in the failure case and breaks side-by-side use of an older build.
- Keep reading `ink.ini` forever: two sources of truth and a permanent branch for a project that has not been released.

## Consequences

One extra file attribute query at startup. The migration code can be removed in a later release when nobody is expected to have an `ink.ini`. It is covered by `test_legacy_migration` in `tests/test_config.c`.
