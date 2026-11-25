/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/ui.h"

#define WORK_WIDTH 1600
#define WORK_HEIGHT 900
#define PROBE_Y 36
#define CENTER_TOLERANCE 3

/**
 * @brief Ignores UI events.
 * @param user Unused.
 * @param event Unused.
 */
static void ignore_event(void *user, const UiEvent *event)
{
    (void)user;
    (void)event;
}

/**
 * @brief Finds the horizontal extent of the toolbar by probing the row it occupies.
 * @param ui UI instance.
 * @param left Receives the first x inside the toolbar.
 * @param right Receives the last x inside the toolbar.
 */
static void toolbar_extent(const Ui *ui, int *left, int *right)
{
    *left = -1;
    *right = -1;
    for (int x = 0; x < WORK_WIDTH; x++) {
        POINT point = {x, PROBE_Y};
        if (ui_contains_point(ui, point)) {
            *left = *left < 0 ? x : *left;
            *right = x;
        }
    }
}

/**
 * @brief Asserts that the toolbar sits in the middle of the work area.
 * @param ui UI instance.
 */
static void assert_centered(const Ui *ui)
{
    int left;
    int right;
    toolbar_extent(ui, &left, &right);
    assert(left > 0 && right > left);
    int difference = left - (WORK_WIDTH - 1 - right);
    assert(difference >= -CENTER_TOLERANCE && difference <= CENTER_TOLERANCE);
}

/**
 * @brief The toolbar stays centered at start, and after tools are hidden and shown again.
 */
static void test_toolbar_stays_centered(void)
{
    UiSettings settings;
    memset(&settings, 0, sizeof settings);
    Ui *ui = ui_create(GetModuleHandleW(NULL), ignore_event, NULL, &settings);
    assert(ui);
    UiModel model;
    memset(&model, 0, sizeof model);
    model.style = style_default();
    tool_order_default(model.toolOrder);
    model.hiddenTools = 1u << TOOL_DIAMOND;
    ui_update(ui, &model);
    RECT work = {0, 0, WORK_WIDTH, WORK_HEIGHT};
    ui_place_defaults(ui, work, false);
    ui_set_visible(ui, true, false);
    assert_centered(ui);
    model.hiddenTools = (1u << TOOL_DIAMOND) | (1u << TOOL_LASER) | (1u << TOOL_ERASER);
    ui_update(ui, &model);
    assert_centered(ui);
    model.hiddenTools = 0;
    ui_update(ui, &model);
    assert_centered(ui);
    ui_destroy(ui);
}

int main(void)
{
    test_toolbar_stays_centered();
    printf("toolbar tests passed\n");
    return 0;
}
