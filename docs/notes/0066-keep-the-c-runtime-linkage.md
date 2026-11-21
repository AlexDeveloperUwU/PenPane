# 0066. The C runtime stays linked

- Status: accepted
- Date: 2026-11-21

## Problem

Dropping the runtime (`-nostartfiles`, own startup, own allocator and string functions) looked like a way to cut memory and size.

## Decision

Keep the default runtime linkage.

## Why

Measured with a custom entry point and `-nostartfiles` (everything else equal): file size 108.5 to 102.9 KB, hidden private bytes 1.60 versus 1.62 MB, drawing private bytes 15.8 versus 15.9 MB, module list unchanged at 38. The Universal CRT is mapped anyway because `gdi32full` and `msvcp_win` depend on it, so removing the application's own imports unloads nothing. The program needs only about 25 runtime functions, but the math functions (`sinf`, `cosf`, `atan2f`) feed pixel positions, and replacing them would risk single-pixel differences for no memory gain.

## Alternatives rejected

- Own `malloc` on `HeapAlloc`: the runtime already forwards to the process heap, which has the low-fragmentation setting by default.
- Replacing `swprintf` with `wsprintfW`: removes one stdio import and changes nothing measurable.

## Consequences

The 5 KB of file size are not worth a hand-written runtime. Revisit only if the executable has to be a few tens of kilobytes.
