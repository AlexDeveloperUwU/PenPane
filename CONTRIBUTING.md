# Contributing to PenPane

Thank you for helping. This guide is short on purpose.

## Licensing of contributions

PenPane is licensed under GPL-3.0-or-later. By sending a contribution you agree that it is distributed under the same license. Every source and script file starts with the line `SPDX-License-Identifier: GPL-3.0-or-later`; keep it on new files.

## Build and test

You need Windows 10 or 11 and PowerShell. Nothing else is installed by hand:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test.ps1
```

The build must produce `build\PenPane.exe` without warnings and every test program must pass before you open a pull request. More in [docs/building.md](docs/building.md). When you change behavior, add or adjust a test in `tests/`; the model, gesture, configuration, pen and hit-testing code is independent of Windows state and is meant to be tested.

## Code style

- C11 against Win32, no third-party libraries, no C++.
- English only in code, documentation and commit messages.
- No inline comments. Document code with Doxygen blocks (`/** ... */`) on files, types and functions: a `@brief`, every `@param` and `@return`. If a line needs a comment to be understood, rename or split it instead.
- The only line comment-like text allowed in a file is the SPDX license line at the top.
- Header guards use `PENPANE_` and the file name, for example `PENPANE_CANVAS_H`.
- Keep the idle behavior: no timers, threads or wake-ups unless something is visibly happening (see [docs/architecture.md](docs/architecture.md)). Check size and idle cost with `scripts\measure.ps1` when you touch the shell or the build.
- Generate the API documentation with `doxygen Doxyfile` to see how your comments render.

## Decision notes

A non-obvious decision gets a note: copy `docs/notes/_template.md` to `docs/notes/NNNN-short-title.md`, fill every section (problem, decision, why, alternatives rejected, consequences) and link to it from the code or document it explains. Note numbers are never reused. The maintainers regenerate `docs/notes/index.md`.

## Commits and pull requests

- Use [Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `perf:`, `refactor:`, `docs:`, `test:`, `build:`, `ci:`, `chore:`, with an optional scope such as `feat(ui):`. Use `!` or a `BREAKING CHANGE:` footer for incompatible changes.
- Keep one logical change per commit and per pull request.
- Add a line under `[Unreleased]` in [CHANGELOG.md](CHANGELOG.md) for every change a user can notice.
- Fill in the pull request template. Describe what you tested and on which Windows version.

## Reporting bugs and ideas

Use the issue templates. For security problems follow [SECURITY.md](SECURITY.md) and do not open a public issue.

## Conduct

Everyone taking part is expected to follow the [code of conduct](CODE_OF_CONDUCT.md).
