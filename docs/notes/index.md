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
