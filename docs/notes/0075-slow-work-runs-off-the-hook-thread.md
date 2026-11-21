# 0075. Disk writes and PNG encoding run on a worker thread

- Status: accepted
- Date: 2026-11-21

## Problem

The low-level mouse hook is called on the thread that owns the message loop, and that same thread saved the configuration (a flushed write-through rename) and encoded the whole drawing as PNG for the clipboard. While it did, the hook could not answer. Windows removes a hook that does not answer in time, silently, and a slow hook also lags the pointer of the whole machine. After a save or a copy, mouse bindings could stop working until restart.

## Decision

`jobs.c` runs two kinds of work on one short-lived worker thread: configuration saves and clipboard copies. A save request copies the configuration and path and replaces any save that has not started, so a burst of panel moves writes once. A copy request takes ownership of an already rendered canvas; rendering stays on the message thread because it uses GDI text. The worker starts on demand and ends when both queues are empty, so an idle process has no extra thread. Shutdown waits up to three seconds for running work.

## Why

The worker needs no message loop and shares nothing but two queue slots guarded by one lock. Replacing pending saves keeps the file consistent and the work small. Rendering on the message thread is a few milliseconds and does not touch the disk.

## Alternatives rejected

- Deferring the work with a timer: it still blocks the thread for its whole duration, only later.
- A permanent worker thread with an event: costs a thread and a stack for the life of the process to save a few writes.

## Consequences

A save is no longer finished when `save_config` returns, so a crash right after a change can lose that change. Process exit waits briefly so a normal quit does not. Clipboard failures are no longer reported to the caller.
