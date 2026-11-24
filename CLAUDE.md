# PenPane

Native C11 screen-annotation overlay for Windows (Win32, no third-party libraries). Full rules: [CONTRIBUTING.md](CONTRIBUTING.md). Layers: [docs/architecture.md](docs/architecture.md).

## Commands

The default toolchain is `C:\tools\llvm-mingw-20261117-ucrt-x86_64` (what `setup-toolchain.ps1` installs). If it lives elsewhere, pass `-Toolchain <path>` to `build`, `test`, `format` and `lint`.

- `scripts\build.ps1` builds `build\PenPane.exe` with `-Werror`.
- `scripts\test.ps1` builds and runs every `tests\test_*.c`.
- `scripts\format.ps1` formats in place; `-Check` only verifies (CI).
- `scripts\lint.ps1` runs clang-tidy; files in `scripts\lint-allowlist.txt` are known debt and the list may only shrink.
- `scripts\bump.ps1 patch|minor|major` bumps `src\version.h` and `Doxyfile` and opens a dated CHANGELOG section.

## Rules

- C11, English only. Doxygen blocks (`@brief`, plus `@param`/`@return` only where the signature does not say it) instead of inline comments; the SPDX line is the only line comment.
- Every file starts with `SPDX-License-Identifier: GPL-3.0-or-later`; header guards are `PENPANE_<FILE>_H`.
- Size limits (clang-tidy): functions at most 60 lines and cognitive complexity 25; files about 600 lines. Do not add to a file on the allowlist beyond what the change needs; split instead.
- One responsibility per module. Pure-logic modules (model, config parsing, gestures, pen, hit-testing) must not gain `windows.h` includes. No new global mutable state.
- Check every allocation, Win32 call and I/O result; never truncate or drop input silently (report it).
- Zero idle cost: no timers, threads or wake-ups unless something is visibly happening.
- A behavior change or bug fix ships with a test in `tests\`. A non-obvious decision gets a note from `docs\notes\_template.md`.
- No test or debug hooks in release code paths.

## Release

No `[Unreleased]` section in `CHANGELOG.md`. Use `bump.ps1`, fill the new section, build, test, then commit signed (Conventional Commits, capitalized description) and push. Never commit or push unasked. The app is not code-signed (no certificate); keep SHA-256 files on releases.
