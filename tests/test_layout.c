/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/ui_internal.h"

/**
 * @brief Tells whether two rectangles share any area.
 * @param a First rectangle.
 * @param b Second rectangle.
 * @return true when they overlap.
 */
static bool overlaps(const RECT *a, const RECT *b)
{
    return a->left < b->right && b->left < a->right && a->top < b->bottom && b->top < a->bottom;
}

/**
 * @brief Counts the labels of a layout whose text equals a string.
 * @param layout Layout to search.
 * @param text Text to find.
 * @return Number of matching labels.
 */
static int count_labels(const UiLayout *layout, const wchar_t *text)
{
    int found = 0;
    for (int i = 0; i < layout->count; i++) {
        found += layout->items[i].kind == ITEM_LABEL && wcscmp(layout->items[i].text, text) == 0;
    }
    return found;
}

/**
 * @brief Labels never share area: a label clears its box before drawing, so overlap would erase text.
 */
static void test_properties_labels_do_not_overlap(void)
{
    static UiLayout layout;
    UiModel model;
    memset(&model, 0, sizeof model);
    model.style = style_default();
    model.sections = UI_SECTION_STROKE | UI_SECTION_FILL | UI_SECTION_WIDTH | UI_SECTION_STROKE_STYLE |
                     UI_SECTION_CORNERS | UI_SECTION_FILL_STYLE | UI_SECTION_OPACITY;
    ui_layout_build(PANEL_PROPERTIES, &model, false, 1.0f, &layout);
    assert(count_labels(&layout, L"Fill style") == 1);
    ui_layout_build(PANEL_PROPERTIES, &model, false, 1.0f, &layout);
    assert(count_labels(&layout, L"Text align") == 0);
    model.sections = UI_SECTION_TEXT_ALIGN;
    ui_layout_build(PANEL_PROPERTIES, &model, false, 1.0f, &layout);
    assert(count_labels(&layout, L"Text align") == 1);
    model.sections = UI_SECTION_STROKE | UI_SECTION_FILL | UI_SECTION_WIDTH | UI_SECTION_STROKE_STYLE |
                     UI_SECTION_CORNERS | UI_SECTION_FILL_STYLE | UI_SECTION_OPACITY;
    ui_layout_build(PANEL_PROPERTIES, &model, false, 1.0f, &layout);
    assert(count_labels(&layout, L"Opacity") == 1);
    assert(count_labels(&layout, L"Corners") == 1);
    for (int i = 0; i < layout.count; i++) {
        for (int j = i + 1; j < layout.count; j++) {
            if (layout.items[i].kind == ITEM_LABEL && layout.items[j].kind == ITEM_LABEL) {
                assert(!overlaps(&layout.items[i].rect, &layout.items[j].rect));
            }
        }
    }
}

/**
 * @brief Every settings tab has the same size, fits its items and keeps labels apart.
 */
static void test_settings_tabs(void)
{
    static UiLayout layout;
    UiModel model;
    memset(&model, 0, sizeof model);
    model.style = style_default();
    int height = 0;
    for (int tab = 0; tab < 4; tab++) {
        model.settingsTab = tab;
        ui_layout_build(PANEL_SETTINGS, &model, false, 1.0f, &layout);
        assert(layout.count > 0 && layout.count < UI_MAX_ITEMS);
        height = height ? height : layout.height;
        assert(layout.height == height);
        for (int i = 0; i < layout.count; i++) {
            assert(layout.items[i].rect.bottom <= layout.height &&
                   layout.items[i].rect.right <= layout.width);
            for (int j = i + 1; j < layout.count; j++) {
                if (layout.items[i].kind == ITEM_LABEL && layout.items[j].kind == ITEM_LABEL) {
                    assert(!overlaps(&layout.items[i].rect, &layout.items[j].rect));
                }
            }
        }
    }
}

int main(void)
{
    test_settings_tabs();
    test_properties_labels_do_not_overlap();
    printf("layout tests passed\n");
    return 0;
}
