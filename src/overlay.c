#include "overlay.h"

#include <string.h>

#define OVERLAY_CLASS_NAME L"InkOverlayWindow"
#define OVERLAY_BASE_STYLE (WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)

bool overlay_create(Overlay *overlay, HINSTANCE instance, RECT bounds, WNDPROC procedure,
                    void *owner)
{
    memset(overlay, 0, sizeof *overlay);
    overlay->bounds = bounds;

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof windowClass;
    windowClass.lpfnWndProc = procedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_CROSS);
    windowClass.lpszClassName = OVERLAY_CLASS_NAME;
    RegisterClassExW(&windowClass);

    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    overlay->window =
        CreateWindowExW(OVERLAY_BASE_STYLE | WS_EX_TRANSPARENT, OVERLAY_CLASS_NAME, L"Ink",
                        WS_POPUP, bounds.left, bounds.top, width, height, NULL, NULL, instance,
                        owner);
    if (!overlay->window) {
        return false;
    }
    SetWindowLongPtrW(overlay->window, GWLP_USERDATA, (LONG_PTR)owner);
    return canvas_create(&overlay->canvas, width, height);
}

void overlay_destroy(Overlay *overlay)
{
    if (overlay->window) {
        DestroyWindow(overlay->window);
    }
    canvas_destroy(&overlay->canvas);
    memset(overlay, 0, sizeof *overlay);
}

void overlay_set_visible(Overlay *overlay, bool visible)
{
    ShowWindow(overlay->window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void overlay_set_capture(Overlay *overlay, bool capture)
{
    LONG_PTR style = GetWindowLongPtrW(overlay->window, GWL_EXSTYLE);
    style = capture ? (style & ~(LONG_PTR)WS_EX_TRANSPARENT) : (style | WS_EX_TRANSPARENT);
    SetWindowLongPtrW(overlay->window, GWL_EXSTYLE, style);
    overlay->capturesInput = capture;
}

void overlay_present(Overlay *overlay, const RECT *dirty)
{
    POINT position = {overlay->bounds.left, overlay->bounds.top};
    SIZE size = {overlay->canvas.width, overlay->canvas.height};
    POINT source = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UPDATELAYEREDWINDOWINFO info = {0};
    info.cbSize = sizeof info;
    info.pptDst = &position;
    info.psize = &size;
    info.hdcSrc = overlay->canvas.dc;
    info.pptSrc = &source;
    info.pblend = &blend;
    info.dwFlags = ULW_ALPHA;
    info.prcDirty = dirty;
    UpdateLayeredWindowIndirect(overlay->window, &info);
}
