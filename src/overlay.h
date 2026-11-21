/**
 * @file overlay.h
 * @brief Full-screen transparent layered window that can capture or ignore input.
 */
#ifndef INK_OVERLAY_H
#define INK_OVERLAY_H

#include <stdbool.h>
#include <windows.h>

#include "canvas.h"

/** @brief Overlay window together with the surface it presents. */
typedef struct Overlay {
    HWND window;
    RECT bounds;
    Canvas canvas;
    bool capturesInput;
} Overlay;

/**
 * @brief Creates the overlay window covering a screen rectangle, initially hidden and click-through.
 * @param overlay Overlay to initialize.
 * @param instance Module instance used to register the window class.
 * @param bounds Screen rectangle in virtual-desktop coordinates.
 * @param procedure Window procedure that receives overlay messages.
 * @param owner Pointer stored as window user data, retrievable with GetWindowLongPtr.
 * @return true on success, false when registration, window or surface creation failed.
 */
bool overlay_create(Overlay *overlay, HINSTANCE instance, RECT bounds, WNDPROC procedure,
                    void *owner);

/**
 * @brief Destroys the window and releases the surface.
 * @param overlay Overlay to destroy.
 */
void overlay_destroy(Overlay *overlay);

/**
 * @brief Shows or hides the window without activating it.
 * @param overlay Target overlay.
 * @param visible true to show, false to hide.
 */
void overlay_set_visible(Overlay *overlay, bool visible);

/**
 * @brief Switches between capturing input and letting it pass through to windows below.
 * @param overlay Target overlay.
 * @param capture true to receive mouse and pen input, false for click-through.
 */
void overlay_set_capture(Overlay *overlay, bool capture);

/**
 * @brief Pushes the canvas to the screen.
 * @param overlay Target overlay.
 * @param dirty Region that changed since the last present, or NULL for the whole surface.
 */
void overlay_present(Overlay *overlay, const RECT *dirty);

#endif
