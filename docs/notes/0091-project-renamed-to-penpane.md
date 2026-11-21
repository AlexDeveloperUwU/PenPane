# 0091. The project is renamed from Ink to PenPane

- Status: accepted
- Date: 2026-11-21

## Problem

The project was called "Ink". The name is a common English word, collides with "Windows Ink" (the Microsoft pen platform whose pointer messages the program itself consumes), and is impossible to search for. Publishing the repository publicly needs a name that identifies the project, can be searched, and does not suggest it is part of Windows.

## Decision

The product is called PenPane: "pen" for the input device that drives it and "pane" for the transparent layer it draws on. The display name is written `PenPane` in prose, user interface text, window titles, the tray tooltip, version information, the changelog and documentation. File names and identifiers are lowercase or upper snake case: `penpane.exe`, `penpane.ini`, `penpane.rc`, `PENPANE_` header guards and macros, `PenPane` as the prefix of window class and synchronization object names. Behavior is unchanged. The registry value for starting with Windows is renamed to `PenPane` and the configuration file migration is described in note 0092.

## Why

The name says what the tool is, is pronounceable, and was free on the places that matter when the checks were run on 2026-11-21:

- GitHub: repository search for `penpane` in repository names returned no result, and neither a user nor an organization named `penpane` exists.
- npm, PyPI and crates.io: no package named `penpane`.
- winget: no package page named `penpane`.
- Domains: `penpane.com`, `.app`, `.dev` and `.io` did not resolve in DNS. That shows they are not in use, not that they are for sale.

## Alternatives rejected

Checked the same day (GitHub repositories with the name in their name, whether a GitHub account of that name exists):

| Name | Repositories | Account | Verdict |
|---|---|---|---|
| Inkglass | 6 | exists | taken, and keeps the generic word "ink" |
| Glasspen | 2 | exists | taken |
| Penlay | 1 | exists | taken, and unclear to read |
| PenScribe | 9 | exists | taken, crowded |
| Pendraw | 17 | exists | taken, crowded |
| ChalkScreen | 0 | free | free, but suggests a classroom chalkboard tool and says nothing about the pen |
| Ink (unchanged) | many | exists | cannot be searched, collides with Windows Ink |

## Consequences

Every reference changes once. A `penpane.exe` next to an old `ink.exe` does not conflict; an existing `ink.ini` is adopted once (note 0092). The autostart registry value of earlier builds is not removed or migrated; the project had not been released. The trademark situation of the name was not investigated beyond the registries above. Repository and domain ownership still have to be claimed by the maintainers.
