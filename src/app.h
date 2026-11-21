/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file app.h
 * @brief Application shell: owns every module, routes input and applies the overlay state machine.
 *
 * States: hidden (no window, no surface), passthrough (annotations visible, input reaches the
 * apps below) and drawing (the overlay captures input). Desktop and whiteboard modes keep
 * separate scenes.
 */
#ifndef PENPANE_APP_H
#define PENPANE_APP_H

#include <windows.h>

/**
 * @brief Runs the application until the user quits.
 * @param instance Module instance.
 * @return Process exit code.
 */
int app_run(HINSTANCE instance);

#endif
