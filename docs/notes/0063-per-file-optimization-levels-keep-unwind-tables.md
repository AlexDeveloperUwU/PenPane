# 0063. Size-optimized build with two faster files; unwind tables stay

- Status: accepted
- Date: 2026-11-21

## Problem

The executable was 105.5 KB and the size target is under 100 KB, while the rasterizer and the blend loop are the only code where speed is measurable.

## Decision

`scripts/build.ps1` compiles every source file to its own object: `-Oz` by default, `-Os` for `raster.c`, `-O2` for `canvas.c`, all with `-flto`, `-ffunction-sections`, `-fdata-sections` and `-funwind-tables`, and links with `-Wl,--gc-sections -s`. Result: 99.3 KB.

## Why

Everything except the rasterizer runs on events and never shows up in a profile. The two hot files cost about 1 KB at these levels.

## Alternatives rejected

- `-Oz` everywhere: 97.3 KB, but a full-screen ellipse took 39 ms before the span work of note 0064 and 9.8 ms after, against 5.3 ms at `-Os`.
- `-O2` for the rasterizer: 102.4 KB, ellipse 4.5 ms; the 3 KB were not worth about 1 ms.
- `-O2` everywhere: 120 KB.
- `-fno-unwind-tables -fno-asynchronous-unwind-tables`: saves 5.6 KB, but the crash filter that hides the overlay never runs. Verified with a throwaway build that dereferences null on a key and writes a marker file from the filter: the marker appears with unwind tables and does not without them, because the exception dispatcher cannot walk frames that have no unwind data. A vectored handler would not need the tables but also sees first-chance exceptions that libraries handle themselves, so it could hide the overlay of a healthy process.
- `--icf=all`, `--disable-reloc-section`, `-fno-stack-protector`: no change in size (file alignment is 512 bytes).

## Consequences

The build makes one compiler call per file instead of one in total. `-Debug` keeps the single call. The tests do not use these flags and are unaffected.
