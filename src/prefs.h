/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file prefs.h
 * @brief Persisted panel preferences, shared by the configuration file and the floating UI.
 *
 * Plain data only, so the configuration module does not depend on the window code.
 */
#ifndef PENPANE_PREFS_H
#define PENPANE_PREFS_H

#include <stdbool.h>

/**
 * @brief The floating islands.
 *
 * PANEL_COUNT counts the panels with saved placement and pin state. The dialogs (settings, clear
 * confirmation, color picker) are extra islands that are centered when they open, never persisted
 * and open or close independently of each other.
 */
typedef enum PanelId {
    PANEL_TOOLBAR,
    PANEL_PROPERTIES,
    PANEL_HISTORY,
    PANEL_COUNT,
    PANEL_SETTINGS = PANEL_COUNT,
    PANEL_CONFIRM,
    PANEL_COLOR,
    PANEL_ISLAND_COUNT
} PanelId;

/** @brief Persisted top-left screen position of one island. */
typedef struct PanelPlacement {
    bool set;
    int x;
    int y;
} PanelPlacement;

/** @brief UI preferences loaded from the configuration file. */
typedef struct UiSettings {
    PanelPlacement placement[PANEL_ISLAND_COUNT];
    bool autoHide;
    int autoHideMs;
    bool excludeFromCapture;
    bool pinned[PANEL_ISLAND_COUNT];
} UiSettings;

#endif
