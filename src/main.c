#include <windows.h>

#include "overlay.h"
#include "raster.h"
#include "theme.h"

/**
 * @brief Draws the milestone 1 demonstration shapes onto the overlay surface.
 * @param overlay Overlay whose canvas receives the shapes.
 */
static void draw_demo_scene(Overlay *overlay)
{
    Canvas *canvas = &overlay->canvas;
    Mask mask;

    if (mask_create(&mask, 100, 100, 420, 300)) {
        mask_add_rect_stroke(&mask, 140, 140, 300, 180, 18, STROKE_WIDTH_BOLD);
        mask_blend(canvas, &mask, COLOR_STROKE_RED);
        mask_destroy(&mask);
    }
    if (mask_create(&mask, 500, 100, 360, 300)) {
        mask_add_ellipse_stroke(&mask, (Point){680, 240}, 140, 90, STROKE_WIDTH_MEDIUM);
        mask_blend(canvas, &mask, COLOR_STROKE_BLUE);
        mask_destroy(&mask);
    }
    if (mask_create(&mask, 100, 420, 600, 120)) {
        mask_add_capsule(&mask, (Point){140, 480}, (Point){560, 500}, 3.0f, 3.0f);
        mask_blend(canvas, &mask, COLOR_STROKE_GREEN);
        mask_destroy(&mask);
    }
}

/**
 * @brief Window procedure of the overlay; only handles destruction for now.
 * @param window Overlay window handle.
 * @param message Window message.
 * @param wparam First message parameter.
 * @param lparam Second message parameter.
 * @return Message result.
 */
static LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show)
{
    (void)previous;
    (void)command;
    (void)show;

    RECT bounds = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    Overlay overlay;
    if (!overlay_create(&overlay, instance, bounds, overlay_procedure, NULL)) {
        return 1;
    }
    RegisterHotKey(NULL, EMERGENCY_HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_ESCAPE);

    draw_demo_scene(&overlay);
    overlay_present(&overlay, NULL);
    overlay_set_visible(&overlay, true);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == EMERGENCY_HOTKEY_ID) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    overlay_destroy(&overlay);
    return 0;
}
