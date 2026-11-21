# 0073. Keys that type text need Ctrl, Alt or Win to be bound globally

- Status: accepted
- Date: 2026-11-21

## Problem

Any key could be bound, including letters, digits, space, punctuation, Esc, Enter, Tab and Backspace. `RegisterHotKey` takes the combination away from every application, so one learned or typed `bind = A single undo` made the letter A unusable system-wide. Learn mode could capture such a key by accident, for example when the user pressed a key on the keyboard instead of the pen button.

## Decision

`trigger_is_bindable` rejects a keyboard trigger whose key is A-Z, 0-9, Space, Esc, Enter, Tab, Backspace or an OEM punctuation key unless Ctrl, Alt or Win is part of it. Shift alone does not count because Shift plus a letter still types. Function keys, navigation keys, the numeric keypad, other virtual keys and every mouse button stay free. The rule is enforced in the configuration parser (the line is reported and skipped), in `config_set_binding`, in learn mode (the press is swallowed and learning ends with a "rejected" result and a notification) and again when hotkeys are registered.

## Why

The risk is not in one place, so one check cannot live in one place: a hand-edited file, a learned key and a table built in code can each reach `RegisterHotKey`. Keeping the predicate next to the configuration, which has no dependence on Windows state, lets it be tested.

## Alternatives rejected

- Rejecting only in learn mode: a hand-edited file would still take over typing.
- Blocking every key without a modifier: pen buttons commonly send plain F13 to F24 or similar keys, which must keep working.

## Consequences

A configuration file written by an older version that contains such a binding loses it with a reported warning. Users who really want a plain letter can add Ctrl or Alt. The table of accepted names in `docs/configuration.md` states the rule.
