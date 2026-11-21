# 0074. An unreadable configuration file is not a missing one and is never overwritten

- Status: accepted
- Date: 2026-11-21

## Problem

`config_load` returned false for every failure to read the file, and the caller treated false as "the file does not exist" and wrote the defaults over it. A file locked by an editor, a transient sharing violation or an access problem therefore replaced the user's settings and bindings with the defaults.

## Decision

`config_load` returns three states: loaded, missing (only `ERROR_FILE_NOT_FOUND`) and unreadable (everything else). On unreadable it leaves the configuration in memory untouched. The application then protects the file: it keeps the settings it has, tells the user, and skips every save until a later reload reads the file successfully. Saves run on the worker thread (note 0075), so the skip is a single flag check.

## Why

Only a clearly absent file justifies creating a new one. For every other error the safe answer is to do nothing to the file, because the user's copy may be intact and recoverable. Keeping the old in-memory settings on a failed reload avoids a surprise reset of bindings in the middle of a session.

## Alternatives rejected

- Retrying the read a few times: hides a lock for a moment but still overwrites on a long lock.
- Writing the defaults to a different file name: leaves the user with two files and unclear precedence.

## Consequences

A missing folder (`ERROR_PATH_NOT_FOUND`) also counts as unreadable and nothing is created there. After a protected start the settings panel moves and tray toggles are not saved until the file is readable again and the user chooses Reload config.
