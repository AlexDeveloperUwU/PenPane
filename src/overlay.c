/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "overlay.h"

#include <string.h>

#define OVERLAY_CLASS_NAME L"PenPaneOverlayWindow"
#define OVERLAY_BASE_STYLE (WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)
#define INPUT_WINDOW_STYLE (WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW)
#define INPUT_WINDOW_ALPHA 1

/**
 * @brief Window that takes input: the input window when present, otherwise the overlay window.
 * @param overlay Target overlay.
 * @return Handle to receive input.
 */
static HWND input_target(const Overlay *overlay)
{
    return overlay->inputWindow ? overlay->inputWindow : overlay->window;
}

bool overlay_create(Overlay *overlay, HINSTANCE instance, RECT bounds, WNDPROC procedure, void *owner)
{
    memset(overlay, 0, sizeof *overlay);
    overlay->bounds = bounds;
    overlay->opacity = 255;

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
        CreateWindowExW(OVERLAY_BASE_STYLE | WS_EX_TRANSPARENT, OVERLAY_CLASS_NAME, L"PenPane", WS_POPUP,
                        bounds.left, bounds.top, width, height, NULL, NULL, instance, owner);
    if (!overlay->window) {
        return false;
    }
    SetWindowLongPtrW(overlay->window, GWLP_USERDATA, (LONG_PTR)owner);
    return true;
}

void overlay_destroy(Overlay *overlay)
{
    if (overlay->inputWindow) {
        DestroyWindow(overlay->inputWindow);
    }
    if (overlay->window) {
        DestroyWindow(overlay->window);
    }
    canvas_destroy(&overlay->canvas);
    memset(overlay, 0, sizeof *overlay);
}

void overlay_set_visible(Overlay *overlay, bool visible)
{
    int command = visible ? SW_SHOWNOACTIVATE : SW_HIDE;
    if (overlay->inputWindow) {
        ShowWindow(overlay->inputWindow, command);
    }
    ShowWindow(overlay->window, command);
}

void overlay_set_capture(Overlay *overlay, bool capture)
{
    HWND target = input_target(overlay);
    LONG_PTR style = GetWindowLongPtrW(target, GWL_EXSTYLE);
    style = capture ? (style & ~(LONG_PTR)WS_EX_TRANSPARENT) : (style | WS_EX_TRANSPARENT);
    SetWindowLongPtrW(target, GWL_EXSTYLE, style);
    overlay->capturesInput = capture;
}

bool overlay_set_input_window(Overlay *overlay, bool enabled, HINSTANCE instance)
{
    if (!enabled) {
        if (overlay->inputWindow) {
            DestroyWindow(overlay->inputWindow);
            overlay->inputWindow = NULL;
        }
        return true;
    }
    if (overlay->inputWindow) {
        return true;
    }
    int width = overlay->bounds.right - overlay->bounds.left;
    int height = overlay->bounds.bottom - overlay->bounds.top;
    HWND input =
        CreateWindowExW(INPUT_WINDOW_STYLE, OVERLAY_CLASS_NAME, L"PenPane", WS_POPUP, overlay->bounds.left,
                        overlay->bounds.top, width, height, NULL, NULL, instance, NULL);
    if (!input) {
        return false;
    }
    SetLayeredWindowAttributes(input, 0, INPUT_WINDOW_ALPHA, LWA_ALPHA);
    SetWindowLongPtrW(input, GWLP_USERDATA, GetWindowLongPtrW(overlay->window, GWLP_USERDATA));
    overlay->inputWindow = input;
    LONG_PTR style = GetWindowLongPtrW(overlay->window, GWL_EXSTYLE);
    SetWindowLongPtrW(overlay->window, GWL_EXSTYLE, style | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    return true;
}

HWND overlay_focus_window(const Overlay *overlay)
{
    return input_target(overlay);
}

void overlay_raise(Overlay *overlay)
{
    const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (overlay->inputWindow) {
        SetWindowPos(overlay->inputWindow, HWND_TOPMOST, 0, 0, 0, 0, flags);
    }
    SetWindowPos(overlay->window, HWND_TOPMOST, 0, 0, 0, 0, flags);
}

void overlay_present(Overlay *overlay, const RECT *dirty)
{
    POINT position = {overlay->bounds.left + overlay->canvas.originX,
                      overlay->bounds.top + overlay->canvas.originY};
    SIZE size = {overlay->canvas.width, overlay->canvas.height};
    POINT source = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, overlay->opacity, AC_SRC_ALPHA};
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

void overlay_set_focusable(Overlay *overlay, bool focusable)
{
    HWND target = input_target(overlay);
    LONG_PTR style = GetWindowLongPtrW(target, GWL_EXSTYLE);
    style = focusable ? (style & ~(LONG_PTR)WS_EX_NOACTIVATE) : (style | WS_EX_NOACTIVATE);
    SetWindowLongPtrW(target, GWL_EXSTYLE, style);
}

void overlay_set_opacity(Overlay *overlay, unsigned char opacity)
{
    overlay->opacity = opacity;
}

void overlay_set_bounds(Overlay *overlay, RECT bounds)
{
    overlay->bounds = bounds;
    if (overlay->inputWindow) {
        SetWindowPos(overlay->inputWindow, NULL, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

bool overlay_set_surface(Overlay *overlay, RECT area)
{
    int width = area.right - area.left;
    int height = area.bottom - area.top;
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (overlay->canvas.pixels && overlay->canvas.width == width && overlay->canvas.height == height) {
        overlay->canvas.originX = area.left;
        overlay->canvas.originY = area.top;
        return true;
    }
    Canvas replacement;
    if (!canvas_create(&replacement, width, height)) {
        return false;
    }
    replacement.originX = area.left;
    replacement.originY = area.top;
    canvas_destroy(&overlay->canvas);
    overlay->canvas = replacement;
    return true;
}
