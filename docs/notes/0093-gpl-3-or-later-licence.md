# 0093. PenPane is licensed under GPL-3.0-or-later

- Status: accepted
- Date: 2026-11-21

## Problem

A public repository needs a license, and without one nobody may legally use or change the code. The project is a local desktop program written in C with no third-party code, so the choice is about what happens to improvements and to derived products.

## Decision

The source is licensed under the GNU General Public License, version 3 or (at the option of the user) any later version. `LICENSE` holds the official text with a copyright line for "PenPane contributors", every source and script file starts with the line `SPDX-License-Identifier: GPL-3.0-or-later`, and the version information of the executable names the license. Contributions are accepted under the same terms (see `CONTRIBUTING.md`).

## Why

Anyone may use, study, change and redistribute the program. Anyone who distributes a modified version, as a program or inside a product, must offer the corresponding source under the same license, so improvements stay available to everybody. The "or later" clause lets the project follow a future version of the license without asking every contributor.

## Alternatives rejected

- MIT: maximum freedom, but a closed fork could take the work and give nothing back.
- Apache-2.0: the same objection, plus an explicit patent grant that matters little for a program of this size.
- AGPL-3.0: extends the source obligation to use over a network. PenPane runs only on the user's own machine and offers no network service, so that extra clause would have nothing to act on and would only discourage some contributors and packagers.
- MPL-2.0: file-level copyleft is a good fit for libraries; PenPane is one application, so whole-program copyleft is simpler to explain and to comply with.
- Source-available licenses: they restrict use and are not open source, which defeats the purpose of publishing it for others to use and improve.

## Consequences

Linking proprietary code into PenPane is not allowed, and the license of any future dependency must be compatible with GPL-3.0-or-later. The toolchain (llvm-mingw) and the Windows libraries are system components and do not affect the license of the program. Third parties may package and sell binaries as long as they provide the source.
