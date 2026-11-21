# 0062. Text services are disabled when only plain keyboard layouts are installed

- Status: accepted
- Date: 2026-11-21

## Problem

In drawing mode the overlay takes keyboard focus. Windows then loads the Text Services Framework for the process: `msctf`, `TextInputFramework`, `CoreMessaging`, `combase` and about a dozen more modules. Measured: 26 modules at start and 38 after the first drawing start, about 9 MB of extra working set and 0.55 MB of private bytes, for an application that only ever receives `WM_CHAR`.

## Decision

`wWinMain` calls `ImmDisableIME(-1)` before any window exists, but only when every installed keyboard layout is an ordinary layout of English, Spanish, Catalan, Galician, Basque, French, German, Italian, Portuguese or Dutch. A layout with the IME marker in its handle, or any other language, keeps text services on.

## Why

Dead keys and accents (`´` then `a`) are resolved by the keyboard layout, not by text services, so typing in the text tool is unchanged. Composition input for Chinese, Japanese and Korean needs text services; the guard keeps it for anyone who has such a layout installed.

## Alternatives rejected

- `ImmAssociateContext(overlay, NULL)`: the modules still load (36 modules measured).
- `ImmDisableTextFrameService(0)`: no effect on the module count (39 measured).
- Disabling unconditionally: removes composition input for CJK users.

## Consequences

Measured drawing state (with the idle trim of note 0061 already in place): working set 24.3 to 15.3 MB, modules 38 to 25, private bytes 15.9 to 15.3 MB. With the guard active the touch keyboard, handwriting panel and emoji picker (which are built on text services) no longer insert text into the text tool; typing on a keyboard works as before (checked by typing "Hola" in the real executable and comparing screenshots). A layout added while the program runs is not noticed until the next start. Remove the call in `src/main.c` to restore full text services.
