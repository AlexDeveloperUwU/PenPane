# 0069. Idle CPU is zero; how it is checked and what the stray cycles are

- Status: accepted
- Date: 2026-11-21

## Problem

The requirement is exactly zero CPU in every state. `TotalProcessorTime` has coarse accounting, so a short burst can hide in it.

## Decision

`scripts\measure.ps1` reports three counters over the idle window: `TotalProcessorTime`, `QueryProcessCycleTime` and context switches summed per thread, plus the thread count. States are measured after a settle time longer than the auto-hide delay, so one-shot timers have already fired.

## Why

Reading the code and the counters agree: one thread of ours blocks in `GetMessage`; timers exist only while a laser or eraser trail is alive (16 ms), while a bound key is held or a gesture is pending (12 ms), as one-shot auto-hide and fade timers, and during learn mode. No hook is installed unless a mouse button is bound or learn mode runs. In every settled state the process shows 0 ms of processor time over 60 s.

Stray cycles that appear in some windows are not ours: Windows starts three or four loader worker threads (state `Wait: EventPairLow`) when a process starts or a library is loaded, and they retire after a while. Their exit shows as 0.3 to 1 ms of cycles with the thread count falling from 3 or 4 to 1, and never repeats in a settled window. `TotalProcessorTime` can also jump by a whole scheduler tick (15.6 ms, seen once as 31.25 ms with only 1.7 million cycles) when such a thread exits, which is why the cycle counter is reported next to it. An empty 65 KB message-loop program shows the same burst, so it is not caused by this application.

## Alternatives rejected

- ETW tracing: heavier than the counters and gives the same answer.
- Counting only `TotalProcessorTime`: cannot see bursts below its resolution.

## Consequences

When a row of the measurement table shows cycles without a thread count change, treat it as a finding. Starting the process, showing the overlay for the first time and loading libraries are the moments that produce the thread bursts.
