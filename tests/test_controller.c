/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/controller.h"

#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600

/** @brief Fixture holding a controller, a scene and a canvas. */
typedef struct Fixture {
    Controller controller;
    Scene scene;
    Canvas canvas;
    uint32_t clock;
} Fixture;

/**
 * @brief Prepares a controller attached to an empty scene and a real in-memory canvas.
 * @param fixture Fixture to initialize.
 */
static void fixture_init(Fixture *fixture)
{
    memset(fixture, 0, sizeof *fixture);
    scene_init(&fixture->scene);
    controller_init(&fixture->controller);
    assert(canvas_create(&fixture->canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    controller_attach(&fixture->controller, &fixture->scene, &fixture->canvas);
}

/**
 * @brief Releases everything owned by a fixture.
 * @param fixture Fixture to release.
 */
static void fixture_destroy(Fixture *fixture)
{
    controller_destroy(&fixture->controller);
    scene_destroy(&fixture->scene);
    canvas_destroy(&fixture->canvas);
}

/**
 * @brief Builds a pointer sample.
 * @param fixture Fixture providing the clock.
 * @param x Horizontal position.
 * @param y Vertical position.
 * @param shift Shift key state.
 * @return Sample.
 */
static PointerSample sample_at(Fixture *fixture, float x, float y, bool shift)
{
    PointerSample sample = {{x, y}, -1.0f, fixture->clock, shift};
    fixture->clock += 8;
    return sample;
}

/**
 * @brief Drags the pointer in a straight line with several intermediate moves.
 * @param fixture Fixture to drive.
 * @param x1 Start x.
 * @param y1 Start y.
 * @param x2 End x.
 * @param y2 End y.
 * @param shift Shift key state.
 */
static void drag(Fixture *fixture, float x1, float y1, float x2, float y2, bool shift)
{
    PointerSample down = sample_at(fixture, x1, y1, shift);
    controller_pointer_down(&fixture->controller, &down);
    for (int i = 1; i <= 10; i++) {
        float t = (float)i / 10.0f;
        PointerSample move = sample_at(fixture, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, shift);
        controller_pointer_move(&fixture->controller, &move, true);
    }
    PointerSample up = sample_at(fixture, x2, y2, shift);
    controller_pointer_up(&fixture->controller, &up);
}

/**
 * @brief Draws a rectangle with the rectangle tool.
 * @param fixture Fixture to drive.
 * @param left Left edge.
 * @param top Top edge.
 * @param right Right edge.
 * @param bottom Bottom edge.
 */
static void draw_rectangle(Fixture *fixture, float left, float top, float right, float bottom)
{
    controller_set_tool(&fixture->controller, TOOL_RECTANGLE);
    drag(fixture, left, top, right, bottom, false);
}

/**
 * @brief The last pointer position becomes the last pen point despite streamlining.
 */
static void test_pen_reaches_release_position(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_PEN);
    drag(&fixture, 100, 100, 300, 140, false);
    assert(fixture.scene.count == 1);
    const Element *pen = &fixture.scene.items[0];
    const PenPoint *last = &pen->points[pen->pointCount - 1];
    assert(fabsf(last->x - 300.0f) < 0.6f && fabsf(last->y - 140.0f) < 0.6f);
    fixture_destroy(&fixture);
}

/**
 * @brief A tap with the pen leaves a single-point dot that can be undone.
 */
static void test_pen_tap_makes_dot(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_PEN);
    PointerSample down = sample_at(&fixture, 50, 50, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample up = sample_at(&fixture, 50, 50, false);
    controller_pointer_up(&fixture.controller, &up);
    assert(fixture.scene.count == 1 && fixture.scene.items[0].pointCount == 1);
    assert(scene_undo(&fixture.scene) && fixture.scene.count == 0);
    fixture_destroy(&fixture);
}

/**
 * @brief The highlighter draws a wide, constant-width pen stroke from its own width table, and switching
 * tools restores the pen widths.
 */
static void test_highlighter_uses_own_widths(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_HIGHLIGHTER);
    assert(fixture.controller.style.highlighter);
    assert(fixture.controller.style.width == HIGHLIGHTER_WIDTHS[1]);
    PointerSample down = {{100, 100}, 0.9f, fixture.clock, false};
    controller_pointer_down(&fixture.controller, &down);
    PointerSample move = {{300, 140}, 0.9f, fixture.clock + 8, false};
    controller_pointer_move(&fixture.controller, &move, true);
    controller_pointer_up(&fixture.controller, &move);
    assert(fixture.scene.count == 1);
    const Element *stroke = &fixture.scene.items[0];
    assert(stroke->type == ELEMENT_PEN && stroke->style.highlighter);
    assert(stroke->style.width == HIGHLIGHTER_WIDTHS[1]);
    assert(stroke->points[0].pressure < 0.05f);
    controller_set_tool(&fixture.controller, TOOL_PEN);
    assert(!fixture.controller.style.highlighter && fixture.controller.style.width == WIDTH_PRESETS[1]);
    fixture_destroy(&fixture);
}

/**
 * @brief Deselecting schedules a repaint of the selection decoration so no ghost is left.
 */
static void test_deselect_repaints_decoration(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_SELECT);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample up = sample_at(&fixture, 100, 150, false);
    controller_pointer_up(&fixture.controller, &up);
    assert(controller_selection(&fixture.controller) != NULL);
    Dirty repaint;
    Dirty direct;
    controller_take_dirty(&fixture.controller, &repaint, &direct);
    RECT decoration = element_selection_bounds(&fixture.scene.items[0]);
    controller_deselect(&fixture.controller);
    controller_take_dirty(&fixture.controller, &repaint, &direct);
    assert(!repaint.empty);
    assert(repaint.rect.left <= decoration.left && repaint.rect.top <= decoration.top);
    assert(repaint.rect.right >= decoration.right && repaint.rect.bottom >= decoration.bottom);
    assert(controller_selection(&fixture.controller) == NULL);
    fixture_destroy(&fixture);
}

/**
 * @brief Moving a selection is one undo step and cancel restores the original geometry.
 */
static void test_move_undo_and_cancel(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_SELECT);
    drag(&fixture, 100, 150, 160, 230, false);
    assert(fabsf(fixture.scene.items[0].from.x - 160.0f) < 0.1f);
    assert(fabsf(fixture.scene.items[0].from.y - 180.0f) < 0.1f);
    assert(scene_undo(&fixture.scene));
    assert(fabsf(fixture.scene.items[0].from.x - 100.0f) < 0.1f);
    assert(scene_redo(&fixture.scene));
    controller_deselect(&fixture.controller);
    PointerSample down = sample_at(&fixture, 160, 230, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample move = sample_at(&fixture, 260, 330, false);
    controller_pointer_move(&fixture.controller, &move, true);
    controller_cancel(&fixture.controller);
    assert(fabsf(fixture.scene.items[0].from.x - 160.0f) < 0.1f);
    assert(fixture.scene.undoCount == 2);
    fixture_destroy(&fixture);
}

/**
 * @brief Resizing from the south-east handle keeps the origin and can be undone.
 */
static void test_resize_handle(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_SELECT);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample up = sample_at(&fixture, 100, 150, false);
    controller_pointer_up(&fixture.controller, &up);
    FloatRect box = element_box(&fixture.scene.items[0]);
    drag(&fixture, box.right + 7.0f, box.bottom + 7.0f, box.right + 47.0f, box.bottom + 27.0f, false);
    FloatRect resized = element_box(&fixture.scene.items[0]);
    assert(fabsf(resized.left - box.left) < 0.1f && fabsf(resized.right - box.right - 40.0f) < 0.1f);
    assert(scene_undo(&fixture.scene));
    FloatRect restored = element_box(&fixture.scene.items[0]);
    assert(fabsf(restored.right - box.right) < 0.1f);
    fixture_destroy(&fixture);
}

/**
 * @brief The eraser marks elements while down, removes them on release and undo restores them.
 */
static void test_eraser_marks_then_removes(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_ERASER);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    assert(fixture.scene.items[0].erasing);
    PointerSample move = sample_at(&fixture, 110, 150, false);
    controller_pointer_move(&fixture.controller, &move, true);
    assert(fixture.scene.count == 1);
    PointerSample up = sample_at(&fixture, 110, 150, false);
    controller_pointer_up(&fixture.controller, &up);
    assert(fixture.scene.count == 0);
    assert(scene_undo(&fixture.scene) && fixture.scene.count == 1 && !fixture.scene.items[0].erasing);
    fixture_destroy(&fixture);
}

/**
 * @brief Cancelling an erase gesture unmarks the elements without removing them.
 */
static void test_eraser_cancel_keeps_elements(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_ERASER);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    controller_cancel(&fixture.controller);
    assert(fixture.scene.count == 1 && !fixture.scene.items[0].erasing);
    fixture_destroy(&fixture);
}

/**
 * @brief Shift makes rectangles square and snaps lines to 15 degree steps.
 */
static void test_shift_constraints(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_RECTANGLE);
    drag(&fixture, 100, 100, 260, 180, true);
    const Element *rectangle = &fixture.scene.items[0];
    assert(fabsf(fabsf(rectangle->to.x - rectangle->from.x) - fabsf(rectangle->to.y - rectangle->from.y)) <
           0.1f);
    controller_set_tool(&fixture.controller, TOOL_LINE);
    drag(&fixture, 400, 400, 500, 405, true);
    const Element *line = &fixture.scene.items[1];
    assert(fabsf(line->to.y - line->from.y) < 0.1f);
    fixture_destroy(&fixture);
}

/**
 * @brief Tiny shapes are discarded and the tool does not report as finished.
 */
static void test_tiny_shape_discarded(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_RECTANGLE);
    drag(&fixture, 100, 100, 101, 101, false);
    assert(fixture.scene.count == 0);
    assert(!controller_take_tool_finished(&fixture.controller));
    drag(&fixture, 100, 100, 200, 200, false);
    assert(fixture.scene.count == 1 && controller_take_tool_finished(&fixture.controller));
    fixture_destroy(&fixture);
}

/**
 * @brief Text editing supports typing, backspace, newline, commit and undo; empty text is dropped.
 */
static void test_text_editing(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_TEXT);
    PointerSample down = sample_at(&fixture, 100, 100, false);
    controller_pointer_down(&fixture.controller, &down);
    assert(controller_is_busy(&fixture.controller));
    const wchar_t *typed = L"Helloo";
    for (const wchar_t *c = typed; *c; c++) {
        assert(controller_text_input(&fixture.controller, *c));
    }
    assert(controller_text_input(&fixture.controller, L'\b'));
    assert(controller_text_input(&fixture.controller, L'\r'));
    assert(controller_text_input(&fixture.controller, L'x'));
    assert(!controller_text_input(&fixture.controller, 1));
    controller_text_commit(&fixture.controller);
    assert(fixture.scene.count == 1);
    assert(wcscmp(fixture.scene.items[0].text, L"Hello\nx") == 0);
    assert(scene_undo(&fixture.scene) && fixture.scene.count == 0);
    PointerSample again = sample_at(&fixture, 200, 200, false);
    controller_pointer_down(&fixture.controller, &again);
    assert(controller_text_input(&fixture.controller, L'\b'));
    controller_text_commit(&fixture.controller);
    assert(fixture.scene.count == 0 && !controller_is_busy(&fixture.controller));
    fixture_destroy(&fixture);
}

/**
 * @brief The laser leaves no element in the scene and its trail dies out, stopping the animation.
 */
static void test_laser_is_transient(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_LASER);
    drag(&fixture, 100, 100, 300, 200, false);
    assert(fixture.scene.count == 0 && !scene_can_undo(&fixture.scene));
    assert(fixture.controller.laser.count > 0);
    uint32_t now = fixture.clock;
    bool alive = true;
    for (int i = 0; i < 400 && alive; i++) {
        now += 16;
        alive = controller_animate(&fixture.controller, now);
    }
    assert(!alive && fixture.controller.laser.count == 0);
    fixture_destroy(&fixture);
}

/**
 * @brief Changing the style of the selection is undoable and leaves defaults to the caller.
 */
static void test_style_change_undo(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_SELECT);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample up = sample_at(&fixture, 100, 150, false);
    controller_pointer_up(&fixture.controller, &up);
    Style style = fixture.scene.items[0].style;
    uint32_t original = style.stroke;
    style.stroke = 0xFFFF0000u;
    controller_apply_style_to_selection(&fixture.controller, &style);
    assert(fixture.scene.items[0].style.stroke == 0xFFFF0000u);
    assert(scene_undo(&fixture.scene));
    assert(fixture.scene.items[0].style.stroke == original);
    fixture_destroy(&fixture);
}

/**
 * @brief Changing the tool while text is being edited commits the text without leaving a stale tool-finished
 * flag.
 */
static void test_tool_change_mid_text_leaves_no_stale_flag(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_TEXT);
    PointerSample down = sample_at(&fixture, 100, 100, false);
    controller_pointer_down(&fixture.controller, &down);
    assert(controller_text_input(&fixture.controller, L'a'));
    controller_set_tool(&fixture.controller, TOOL_RECTANGLE);
    assert(fixture.scene.count == 1);
    assert(!controller_take_tool_finished(&fixture.controller));
    controller_set_tool(&fixture.controller, TOOL_TEXT);
    controller_pointer_down(&fixture.controller, &down);
    assert(controller_text_input(&fixture.controller, L'b'));
    controller_text_commit(&fixture.controller);
    assert(controller_take_tool_finished(&fixture.controller));
    fixture_destroy(&fixture);
}

/**
 * @brief Backspace removes a whole surrogate pair at once and a single unit otherwise.
 */
static void test_backspace_removes_surrogate_pair(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    controller_set_tool(&fixture.controller, TOOL_TEXT);
    PointerSample down = sample_at(&fixture, 100, 100, false);
    controller_pointer_down(&fixture.controller, &down);
    assert(controller_text_input(&fixture.controller, L'a'));
    assert(controller_text_input(&fixture.controller, 0xD83D));
    assert(controller_text_input(&fixture.controller, 0xDE00));
    assert(controller_text_input(&fixture.controller, L'\b'));
    assert(wcscmp(fixture.controller.draft.text, L"a") == 0);
    assert(controller_text_input(&fixture.controller, L'\b'));
    assert(wcscmp(fixture.controller.draft.text, L"") == 0);
    controller_text_commit(&fixture.controller);
    fixture_destroy(&fixture);
}

/**
 * @brief A preview-and-commit style edit, as a slider drag does, is one undo step however many previews it
 * had.
 */
static void test_style_drag_is_one_undo_step(void)
{
    Fixture fixture;
    fixture_init(&fixture);
    draw_rectangle(&fixture, 100, 100, 300, 200);
    controller_set_tool(&fixture.controller, TOOL_SELECT);
    PointerSample down = sample_at(&fixture, 100, 150, false);
    controller_pointer_down(&fixture.controller, &down);
    PointerSample up = sample_at(&fixture, 100, 150, false);
    controller_pointer_up(&fixture.controller, &up);
    int steps = fixture.scene.undoCount;
    float original = fixture.scene.items[0].style.opacity;
    Style style = fixture.scene.items[0].style;
    for (int i = 1; i <= 5; i++) {
        style.opacity = 1.0f - 0.1f * (float)i;
        controller_style_preview(&fixture.controller, &style);
    }
    assert(fixture.scene.undoCount == steps);
    controller_style_commit(&fixture.controller);
    assert(fixture.scene.undoCount == steps + 1);
    assert(fabsf(fixture.scene.items[0].style.opacity - 0.5f) < 0.001f);
    assert(scene_undo(&fixture.scene));
    assert(fixture.scene.items[0].style.opacity == original);
    fixture_destroy(&fixture);
}

/**
 * @brief Entry point.
 * @return 0 when every test passes.
 */
int main(void)
{
    test_tool_change_mid_text_leaves_no_stale_flag();
    test_backspace_removes_surrogate_pair();
    test_style_drag_is_one_undo_step();
    test_pen_reaches_release_position();
    test_pen_tap_makes_dot();
    test_highlighter_uses_own_widths();
    test_deselect_repaints_decoration();
    test_move_undo_and_cancel();
    test_resize_handle();
    test_eraser_marks_then_removes();
    test_eraser_cancel_keeps_elements();
    test_shift_constraints();
    test_tiny_shape_discarded();
    test_text_editing();
    test_laser_is_transient();
    test_style_change_undo();
    printf("controller tests passed\n");
    return 0;
}
