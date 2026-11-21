# Measurements

CPU and memory of the real `build\penpane.exe`, before and after the optimization pass. Screen 2560x1376 at 100 percent, one display, Windows 11, release build. Every number comes from `scripts\measure.ps1` unless a row says otherwise.

## Method

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -IdleSeconds 60
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -Only startup -StartupRuns 15
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -Only leak -Cycles 500
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -Exe path\to\other\penpane.exe -Json result.json
```

The script copies the executable into a scratch directory with a `penpane.ini` that sets `tool_lock = true` and binds `F22` to `clear`, so every run starts from the same configuration. It drives the program only through its public surface: a second launch toggles drawing, `SetCursorPos` and `mouse_event` draw strokes, `keybd_event` sends `Ctrl+Z`, `F22`, a double `F14` (whiteboard), `K` and `P` (laser and pen) and finally `Ctrl+Alt+Shift+Esc`. A WinForms window under the overlay counts the clicks that reach it, once with drawing off (must be 1) and once with drawing on (must be 0).

Per state the script waits for a settle time (8 s, longer than the 1.2 s auto-hide delay, so the one-shot timers have fired), reads memory with `GetProcessMemoryInfo` (`PROCESS_MEMORY_COUNTERS_EX2`: working set, private working set, private bytes), then holds still for `-IdleSeconds` and reports deltas of:

- `TotalProcessorTime`,
- `QueryProcessCycleTime` (sees bursts far below the time accounting resolution),
- context switches summed per thread from `Win32_PerfRawData_PerfProc_Thread`,
- thread count.

Other checks that are not in the script and were done by hand:

- Thread states and start addresses with `Get-Process ... .Threads`, per-thread processor time every 250 ms (`timeline` experiment, see note 0069).
- Working set composition per memory region with `VirtualQueryEx` and `QueryWorkingSetEx` (module, mapped or private, shared or not).
- Latency: a throwaway copy of `src` was patched with `QueryPerformanceCounter` around the pointer handler, around `overlay_present`, around `app_apply_state`, `render_everything` and `app_run`, built with the same flags as the release build and driven by a stroke script; results were written to a file at exit. The patch is not part of the repository.
- Rasterizer: a throwaway benchmark linking `canvas`, `raster`, `render`, `pen` and friends renders 3000 incremental pen events, a 3000-point stroke, 200 strokes, a full-screen ellipse and a full-screen filled rectangle on a 2560x1376 canvas. Output equality: `tests\render_preview.c` through `scripts\preview.ps1` (byte for byte, all seven images) and a comparison of 4000 random shapes against the previous rasterizer (0 differing pixels).

## Before and after

Idle CPU is 0 ms over 60 s in every state, before and after. Memory per state (MB):

| State | WS before | WS after | Private WS before | Private WS after | Private bytes before | Private bytes after |
|---|---|---|---|---|---|---|
| Just started, hidden, +1.5 s | 10.04 | 0.33 | 1.15 | 0.11 | 1.61 | 1.58 |
| Hidden, toolbar handle only | 10.04 | 0.34 | 1.15 | 0.11 | 1.61 | 1.58 |
| Drawing, empty desktop | 29.43 | 15.35 | 15.18 | 14.01 | 15.78 | 15.19 |
| Drawing, 2 strokes | 29.46 | 17.44 | 15.20 | 14.32 | 15.78 | 15.33 |
| Drawing, after laser faded | 29.68 | 0.89 | 15.20 | 0.30 | 15.77 | 15.33 |
| Laser active, trail alive | 29.90 | 3.19 | 15.18 | 1.00 | 15.75 | 15.43 |
| Passthrough with 2 strokes | 43.59 | 0.20 | 15.20 | 0.09 | 15.81 | 2.35 |
| Whiteboard, empty | 29.94 | 15.93 | 15.24 | 14.25 | 15.84 | 15.39 |
| Whiteboard, 1 stroke | 29.99 | 16.02 | 15.30 | 14.31 | 15.88 | 15.35 |
| Whiteboard, drawing off | 16.40 | 0.19 | 1.71 | 0.07 | 2.24 | 1.75 |
| Desktop, drawing off, empty | 16.70 | 0.20 | 1.71 | 0.09 | 2.23 | 1.72 |
| Drawing, empty (second run) | 30.41 | 16.18 | 15.70 | 14.43 | 16.31 | 15.75 |
| Drawing after 200 strokes | 30.97 | 17.77 | 16.71 | 14.66 | 17.29 | 15.68 |
| Passthrough with 200 strokes | 44.38 | 0.18 | 16.64 | 0.07 | 17.22 | 12.01 |
| After 200 undos | 30.45 | 16.49 | 15.93 | 14.50 | 16.52 | 15.69 |
| After clear | 30.29 | 16.65 | 15.77 | 14.44 | 16.41 | 15.65 |

In the drawing rows the surface (13.8 MB) is the private working set; it is not trimmed while the user draws. "Passthrough with 200 strokes" is the worst case for the compact surface: the random strokes cover almost the whole screen, so the surface is 12 MB; its working set is low only because the idle trim ran.

Other quantities:

| Quantity | Before | After |
|---|---|---|
| Executable size | 105,472 bytes | 99,328 bytes |
| Process start to first visible window (median of 15) | 63.5 ms | 47.5 ms |
| Loaded modules, hidden / drawing | 26 / 38 | 25 / 25 |
| Pointer event, handler to present return (average of about 620 events) | 0.257 ms | 0.245 ms |
| Of which `UpdateLayeredWindowIndirect` | 0.200 ms | 0.200 ms |
| Worst pointer event | 1.5 ms | 1.3 ms |
| Show drawing from hidden (create window and surface, render, present) | 79.6 ms | 52.5 ms |
| Show drawing from passthrough | 41.2 ms | 60.0 ms |
| Drawing off to passthrough | 27.0 ms | 23.6 ms |
| Switch to whiteboard from drawing | 11.5 ms | 10.8 ms |
| Show whiteboard from hidden | 56.6 ms | 53.1 ms |
| Processor time of a 2000-move stroke (about 32 s of wall time) | 31.2 ms | 31.3 ms |
| Draw, undo and clear, 500 balanced cycles: private bytes after warm-up, then at cycle 500 | 17.6, then 17.2 | 15.4, then 16.2 |

Notes on these rows:

- The long stroke costs about 15 microseconds of processor time per move, nearly all of it in the kernel and in the present call, so the faster rasterizer does not change it. A pen event spends roughly 0.2 ms in `UpdateLayeredWindowIndirect` and 0.025 ms in the rasterizer.
- Showing drawing from passthrough is slower than before because the compact surface has to be replaced by a full one (note 0060). Showing it from hidden is faster, mostly because text services no longer load (note 0062).
- The leak cycle is draw, draw, clear, undo, undo, undo, so history is balanced and the redo stack is dropped by the next stroke. Private bytes rise during the first 100 cycles (heap and mask buffers reaching their high-water mark) and stay flat afterwards in both builds; nothing grows with the number of cycles. Unbalanced work (strokes kept in the undo history) grows by design: unlimited undo is stored as element copies, about 5 KB per stroke.
- Cycles in the idle windows: every row reads 0 ms of processor time. The cycle counter shows 0.3 to 6 million cycles (0.1 to 2 ms) in some 60 s windows of both builds; they come with a change of the thread count (loader worker threads retiring or being created by Windows) and also occur in an empty 65 KB message-loop program (a 1.8 million cycle burst after 30 s). See note 0069.

## What each change contributed

| Change | Note | Effect |
|---|---|---|
| Passthrough surface covers the annotation extent | 0060 | Passthrough private bytes 15.8 to 2.4 MB, working set 43.6 to 0.2 MB (with 2 strokes); 17.2 to 12.0 MB for 200 strokes spread over the screen |
| Idle working set and scratch trim | 0061 | Hidden working set 10.0 to 0.3 MB; drawing off 16.4 to 0.2 MB; drawing after the laser 29.7 to 0.9 MB |
| Text services off for plain layouts | 0062 | Drawing working set 29.4 to 15.4 MB, modules 38 to 25, private bytes 15.8 to 15.2 MB |
| Per-file optimization levels | 0063 | Executable 105.5 to 99.3 KB |
| Rasterizer spans, specialization, blend fast paths | 0064 | Full-screen ellipse 19.6 to 5.3 ms, rectangle with fill 15.1 to 7.9 ms, 200-stroke repaint 67 to 51 ms, identical pixels |
| Tray icon after the toolbar handle | 0065 | Start to visible window 63.5 to 47.5 ms |

## Tried and rejected

| Idea | Measured | Note |
|---|---|---|
| `-nostartfiles` without the C runtime startup | 108.5 to 102.9 KB, private bytes 1.60 versus 1.62 MB, same 38 modules | 0066 |
| Remove `wtsapi32` | 4 fewer modules, 0.14 MB less private bytes, session unlock not verifiable | 0067 |
| `ImmAssociateContext` on the overlay, `ImmDisableTextFrameService` | 36 and 39 modules, no gain | 0062 |
| `-fno-unwind-tables` | 5.6 KB smaller, but the crash filter stops running | 0063 |
| `-Oz` everywhere | 97.3 KB, but a full-screen ellipse 39 ms before the span work | 0063 |
| `-O2` for the rasterizer, or everywhere | 102 KB, 120 KB | 0063 |
| Two-window drawing with a surface-free capture layer | about 13 MB less while drawing, not attempted for risk | 0068 |

## Remaining ideas

- Drawing mode still holds one 14 MB surface (note 0068). A constant-alpha capture window plus a content window sized to the strokes would remove most of it.
- Export to the clipboard allocates a full-monitor canvas for the duration of the copy; with the canvas origin it could allocate only the annotation extent.
- Replace `wtsapi32` once unlock handling can be verified on a real session.
- The remaining startup time is process creation and loader work; the in-process part is about 10 ms.
