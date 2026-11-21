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
    unsigned char opacity;
} Overlay;

/**
 * @brief Creates the overlay window for a monitor rectangle, initially hidden and click-through, without a surface.
 * @param overlay Overlay to initialize.
 * @param instance Module instance used to register the window class.
 * @param bounds Screen rectangle in virtual-desktop coordinates.
 * @param procedure Window procedure that receives overlay messages.
 * @param owner Pointer stored as window user data, retrievable with GetWindowLongPtr.
 * @return true on success, false when registration or window creation failed.
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
 * @brief Allows or forbids the window to take keyboard focus.
 * @param overlay Target overlay.
 * @param focusable true while the user is actively drawing and shortcuts must reach the overlay.
 */
void overlay_set_focusable(Overlay *overlay, bool focusable);

/**
 * @brief Sets the whole-surface opacity used on the next present, for fading annotations out.
 * @param overlay Target overlay.
 * @param opacity 0 (invisible) to 255 (normal).
 */
void overlay_set_opacity(Overlay *overlay, unsigned char opacity);

/**
 * @brief Records the screen rectangle of the monitor the overlay belongs to.
 *
 * The surface is not touched; call overlay_set_surface afterwards.
 * @param overlay Target overlay.
 * @param bounds New monitor rectangle in virtual-desktop coordinates.
 */
void overlay_set_bounds(Overlay *overlay, RECT bounds);

/**
 * @brief Makes the surface cover a rectangle of the monitor, reallocating it only when the size changes.
 *
 * Drawing coordinates stay relative to the monitor; the canvas origin is set to the rectangle corner.
 * The caller must repaint and present afterwards.
 * @param overlay Target overlay.
 * @param area Rectangle in monitor coordinates, from empty to the whole monitor.
 * @return false when the rectangle is empty or the surface could not be created; the old surface is kept.
 */
bool overlay_set_surface(Overlay *overlay, RECT area);

/**
 * @brief Pushes the canvas to the screen at its origin inside the monitor.
 * @param overlay Target overlay.
 * @param dirty Region that changed since the last present, or NULL for the whole surface.
 */
void overlay_present(Overlay *overlay, const RECT *dirty);

#endif
