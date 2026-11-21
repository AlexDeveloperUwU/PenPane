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

Produces `build\ink.exe`. Add `-Debug` for an unoptimized build with symbols.

## Emergency exit

`Ctrl+Alt+Shift+Esc` always closes the application.
