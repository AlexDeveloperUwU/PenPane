# Decision notes

One note per non-obvious decision, written from `_template.md`.

| Note | Decision |
|---|---|
| [0001](0001-native-c-win32-and-software-rasterizer.md) | Native C, Win32 and a software rasterizer |
| [0002](0002-alpha-one-backdrop-keeps-overlay-hit-testable.md) | Alpha-1 backdrop keeps the overlay hit-testable |
| [0003](0003-layered-window-per-pixel-alpha-and-dirty-present.md) | Layered window with a premultiplied DIB and dirty-rectangle presents |
| [0004](0004-sdf-rasterizer-with-max-coverage-masks.md) | Signed distance rasterizer with per-element maximum-coverage masks |
| [0005](0005-undo-as-grouped-operations-not-pixels.md) | Undo as grouped operations on editable elements |
| [0006](0006-focus-only-while-drawing.md) | Focus is taken only while drawing, and restored afterwards |
| [0007](0007-tool-lock-on-by-default.md) | Tool lock is on by default |
| [0008](0008-second-launch-toggles-drawing.md) | A second launch toggles drawing on the running instance |
| [0009](0009-surface-only-while-visible.md) | The drawing surface only exists while the overlay is visible |
| [0010](0010-registerhotkey-instead-of-keyboard-hook.md) | RegisterHotKey instead of a global keyboard hook |
| [0011](0011-release-detection-by-polling.md) | Release detection by polling while a key is held |
| [0012](0012-learn-mode-hook-lifetime.md) | Learn mode installs hooks only for its duration |
| [0013](0013-gesture-delay-only-for-multi-gesture-keys.md) | Gesture delay only for keys that need it |
| [0014](0014-mouse-triggers-via-deferred-hook.md) | Mouse triggers through a conditional low-level hook with deferred handling |
| [0015](0015-plain-text-configuration-file.md) | A plain-text key/value configuration file |
| [0020](0020-separate-island-windows.md) | Three separate island windows instead of one UI window |
| [0021](0021-capture-exclusion-with-display-affinity.md) | Excluding the tool islands from screen capture with WDA_EXCLUDEFROMCAPTURE |
| [0022](0022-islands-collapse-and-free-memory.md) | Islands collapse to a handle and free their pixels when hidden |
| [0023](0023-mask-based-gdi-text.md) | Text is rasterized by GDI into masks and blended by our canvas |
| [0024](0024-present-dirty-rectangles-with-explicit-geometry.md) | Dirty updates pass position and size to UpdateLayeredWindowIndirect |
| [0030](0030-pen-stroke-ends-at-release-position.md) | A pen stroke ends exactly where the pointer was released |
| [0031](0031-deselect-schedules-decoration-repaint.md) | Dropping the selection always repaints its decoration |
| [0032](0032-tool-lock-default-follows-documentation.md) | Tool lock is off by default |
| [0040](0040-pen-width-model-follows-measured-perfect-freehand.md) | Pen width model follows measured perfect-freehand |
| [0041](0041-shape-strokes-quadratic-corners-and-visible-width.md) | Shape strokes: quadratic corners, 1.5 px thin width and exact dash periods |
| [0042](0042-trails-without-glow-and-with-width-taper.md) | Laser and eraser trails: no glow, width taper by age and point distance |
| [0043](0043-selection-look-and-corner-handles.md) | Selection look: thin outline and four corner handles |
| [0044](0044-coverage-rasterizer-keeps-linear-blend.md) | Rasterizer keeps linear coverage blending and limits capsule work per row |
| [0050](0050-pen-width-mode-follows-device-pressure.md) | Pen width follows real device pressure, otherwise stays constant |
| [0060](0060-passthrough-surface-covers-annotation-extent.md) | The passthrough surface covers only the extent of the annotations |
| [0061](0061-idle-moments-trim-the-working-set.md) | Idle moments give memory back: working set, mask scratch buffer |
| [0062](0062-text-services-off-for-plain-layouts.md) | Text services are disabled when only plain keyboard layouts are installed |
| [0063](0063-per-file-optimization-levels-keep-unwind-tables.md) | Size-optimized build with two faster files; unwind tables stay |
| [0064](0064-rasterizer-interior-spans-and-blend-fast-paths.md) | Rasterizer fast paths that produce identical pixels |
| [0065](0065-tray-icon-after-the-toolbar-handle.md) | The tray icon is registered after the toolbar handle is visible |
| [0066](0066-keep-the-c-runtime-linkage.md) | The C runtime stays linked |
| [0067](0067-keep-wtsapi32-for-session-unlock.md) | Session notifications stay on wtsapi32 |
| [0068](0068-rejected-two-window-drawing-and-other-ideas.md) | Rejected: a surface-free capture layer, a custom heap, other micro-savings |
| [0069](0069-idle-cpu-is-zero-and-how-it-is-measured.md) | Idle CPU is zero; how it is checked and what the stray cycles are |
| [0070](0070-hidden-toplevel-window-instead-of-message-only.md) | A hidden top-level popup owns timers, hotkeys and system notifications |
| [0071](0071-tray-icon-object-survives-failed-registration.md) | The tray icon object survives a failed registration and is retried |
| [0072](0072-emergency-exit-fallback-chain.md) | The emergency exit has a fallback chain and tells the user when it is degraded |
| [0073](0073-typing-keys-need-a-modifier-to-be-bound.md) | Keys that type text need Ctrl, Alt or Win to be bound globally |
| [0074](0074-unreadable-configuration-is-not-missing.md) | An unreadable configuration file is not a missing one and is never overwritten |
| [0075](0075-slow-work-runs-off-the-hook-thread.md) | Disk writes and PNG encoding run on a worker thread |
| [0076](0076-held-mouse-binding-recovery-without-polling-async-state.md) | A stuck mouse binding is recovered by hook liveness and resume resets, not by polling button state |
| [0077](0077-monitor-index-fallback-does-not-rewrite-configuration.md) | A missing monitor is handled by a fallback, not by rewriting the configuration |
| [0078](0078-slider-drag-is-one-undo-step.md) | A slider drag is a single undo step |
| [0079](0079-pointer-identity-and-stale-pointer-recovery.md) | Only the pointer that started a stroke may continue or end it |
| [0080](0080-cancel-discards-the-tool-finished-flag.md) | Cancelling an interaction discards the tool-finished flag |
| [0081](0081-toolchain-download-is-verified-by-checksum.md) | The toolchain download is verified against a pinned SHA-256 |
| [0090](0090-panels-open-on-click-and-can-be-pinned.md) | Panels open on click and can be pinned |
| [0091](0091-project-renamed-to-penpane.md) | The project is renamed from Ink to PenPane |
| [0092](0092-legacy-ink-ini-is-adopted-once.md) | A legacy ink.ini is adopted once |
| [0093](0093-gpl-3-or-later-licence.md) | PenPane is licensed under GPL-3.0-or-later |
| [0094](0094-hatch-fills-are-stripes-cut-from-the-fill-mask.md) | Hatch fills are stripes cut from the fill mask |
| [0095](0095-line-and-arrow-are-one-tool.md) | Line and arrow are one tool |
| [0096](0096-shape-labels-edit-in-place.md) | Shape labels are edited in place and opened by the controller |
