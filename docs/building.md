# Building

The project builds with clang from llvm-mingw (UCRT, x86-64). Nothing else is required.

## Toolchain

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1
```

Installs llvm-mingw into `C:\tools`. Only the project folder persists between sandbox sessions, so rerun this after a reset.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Produces `build\ink.exe` (about 99 KB). Each source file is compiled on its own: `-Oz` by default, `-Os` for the rasterizer and `-O2` for the blend loop, with link-time optimization and unwind tables kept (note 0063). Add `-Debug` for an unoptimized build with symbols.

## Measuring

```powershell
powershell -ExecutionPolicy Bypass -File scripts\measure.ps1 -IdleSeconds 60
```

Drives the real executable with synthetic input and reports idle CPU, context switches, working set, private working set and private bytes per state, plus startup time, a long stroke and a draw, undo and clear leak cycle. `-Only startup,states,strokes,longstroke,leak,exit` selects parts and `-Exe` measures another build. Results and method are in [measurements.md](measurements.md).

## Emergency exit

`Ctrl+Alt+Shift+Esc` always closes the application.
