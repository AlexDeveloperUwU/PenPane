# 0001. Native C, Win32 and a software rasterizer

- Status: accepted
- Date: 2026-11-21

## Problem

The tool must start instantly, idle at near zero CPU, use little memory, ship as one small executable and look like Excalidraw in dark mode. It also has to render over the desktop with per-pixel transparency.

## Decision

Write the application in C11 against the plain Win32 API. Render with our own anti-aliased rasterizer into a 32-bit premultiplied DIB and present it with `UpdateLayeredWindow`. No UI toolkit, no browser engine, no managed runtime.

## Why

A layered window with per-pixel alpha is the cheapest way Windows offers to draw over the desktop, and it needs no GPU. A software rasterizer for a handful of shapes (polylines, ellipses, rectangles, arrowheads) is a few hundred lines. Everything the app draws, including the toolbar, is vector shapes plus text, so a toolkit adds nothing but weight.

## Alternatives rejected

- Electron or WebView2: 100 MB or more of memory and a background process tree. Violates the core goal.
- .NET, WPF, Qt: a runtime or large DLLs, tens of MB resident, slower startup.
- Direct2D: better quality for free, but loads a large set of DLLs, uses the GPU and adds ten to twenty MB of working set.
- Rust: a fine option, but a larger toolchain and larger binaries for no capability we need. C is the smaller path and was not blocked by anything.

## Consequences

We own the rasterizer and must keep it correct and fast, with dirty-rectangle redraw. UI polish (shadows, anti-aliasing, hover transitions) is our work, not a toolkit feature. Memory is dominated by the full-screen pixel buffer, so buffers are only held while the overlay is visible.
