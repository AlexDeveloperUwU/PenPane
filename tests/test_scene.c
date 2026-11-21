#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/hittest.h"
#include "../src/pen.h"
#include "../src/scene.h"

/**
 * @brief Builds a rectangle element for tests.
 * @param left Left edge.
 * @param top Top edge.
 * @param right Right edge.
 * @param bottom Bottom edge.
 * @return Rectangle element with the default style.
 */
static Element make_rectangle(float left, float top, float right, float bottom)
{
    Style style = style_default();
    Element element = element_make(ELEMENT_RECTANGLE, &style);
    element.from = (Point){left, top};
    element.to = (Point){right, bottom};
    return element;
}

/**
 * @brief Add, undo and redo restore the element list exactly.
 */
static void test_add_undo_redo(void)
{
    Scene scene;
    scene_init(&scene);
    Element first = make_rectangle(0, 0, 10, 10);
    Element second = make_rectangle(20, 20, 40, 40);
    assert(scene_add(&scene, &first));
    scene_commit(&scene);
    assert(scene_add(&scene, &second));
    scene_commit(&scene);
    assert(scene.count == 2);
    assert(scene_undo(&scene));
    assert(scene.count == 1);
    assert(scene_undo(&scene));
    assert(scene.count == 0);
    assert(!scene_undo(&scene));
    assert(scene_redo(&scene));
    assert(scene_redo(&scene));
    assert(scene.count == 2);
    assert(!scene_redo(&scene));
    scene_destroy(&scene);
}

/**
 * @brief A new commit discards the redo stack.
 */
static void test_commit_clears_redo(void)
{
    Scene scene;
    scene_init(&scene);
    Element first = make_rectangle(0, 0, 10, 10);
    assert(scene_add(&scene, &first));
    scene_commit(&scene);
    assert(scene_undo(&scene));
    assert(scene_can_redo(&scene));
    Element second = make_rectangle(5, 5, 15, 15);
    assert(scene_add(&scene, &second));
    scene_commit(&scene);
    assert(!scene_can_redo(&scene));
    scene_destroy(&scene);
}

/**
 * @brief Clear is one undoable step that restores order.
 */
static void test_clear_is_undoable(void)
{
    Scene scene;
    scene_init(&scene);
    for (int i = 0; i < 3; i++) {
        Element element = make_rectangle((float)i * 30, 0, (float)i * 30 + 20, 20);
        assert(scene_add(&scene, &element));
        scene_commit(&scene);
    }
    scene_clear(&scene);
    assert(scene.count == 0);
    assert(scene_undo(&scene));
    assert(scene.count == 3);
    assert(scene.items[0].from.x == 0 && scene.items[1].from.x == 30 && scene.items[2].from.x == 60);
    assert(scene_redo(&scene));
    assert(scene.count == 0);
    scene_destroy(&scene);
}

/**
 * @brief A recorded in-place change undoes to the old geometry and redoes to the new.
 */
static void test_record_change(void)
{
    Scene scene;
    scene_init(&scene);
    Element element = make_rectangle(0, 0, 10, 10);
    assert(scene_add(&scene, &element));
    scene_commit(&scene);
    Element before;
    assert(element_copy(&before, &scene.items[0]));
    element_translate(&scene.items[0], 100, 50);
    scene_record_change(&scene, 0, &before);
    scene_commit(&scene);
    assert(scene.items[0].from.x == 100);
    assert(scene_undo(&scene));
    assert(scene.items[0].from.x == 0);
    assert(scene_redo(&scene));
    assert(scene.items[0].from.x == 100 && scene.items[0].from.y == 50);
    scene_destroy(&scene);
}

/**
 * @brief Removing several elements in one step undoes together.
 */
static void test_grouped_remove(void)
{
    Scene scene;
    scene_init(&scene);
    for (int i = 0; i < 4; i++) {
        Element element = make_rectangle((float)i * 30, 0, (float)i * 30 + 20, 20);
        assert(scene_add(&scene, &element));
        scene_commit(&scene);
    }
    scene_remove(&scene, 2);
    scene_remove(&scene, 0);
    scene_commit(&scene);
    assert(scene.count == 2);
    assert(scene_undo(&scene));
    assert(scene.count == 4);
    assert(scene.items[0].from.x == 0 && scene.items[2].from.x == 60);
    scene_destroy(&scene);
}

/**
 * @brief Pen strokes stay editable and survive copying.
 */
static void test_pen_copy(void)
{
    Style style = style_default();
    Element stroke = element_make(ELEMENT_PEN, &style);
    PenBuilder builder;
    assert(pen_begin(&builder, &stroke, (Point){10, 10}, -1.0f));
    for (int i = 1; i < 40; i++) {
        pen_add(&builder, &stroke, (Point){10.0f + (float)i * 3.0f, 10.0f + (float)(i % 5)}, -1.0f);
    }
    assert(stroke.pointCount > 10);
    Element copy;
    assert(element_copy(&copy, &stroke));
    assert(copy.pointCount == stroke.pointCount);
    assert(copy.points != stroke.points);
    assert(hit_element(&copy, (Point){40, 12}, 4.0f));
    assert(!hit_element(&copy, (Point){40, 200}, 4.0f));
    element_free(&copy);
    element_free(&stroke);
}

/**
 * @brief Hit testing: unfilled rectangles hit only on the border, filled ones inside too.
 */
static void test_hit_rectangle(void)
{
    Element rectangle = make_rectangle(100, 100, 200, 200);
    assert(hit_element(&rectangle, (Point){100, 150}, 4.0f));
    assert(!hit_element(&rectangle, (Point){150, 150}, 4.0f));
    rectangle.style.fill = COLOR_FILL_BLUE;
    assert(hit_element(&rectangle, (Point){150, 150}, 4.0f));
    assert(!hit_element(&rectangle, (Point){300, 300}, 4.0f));
}

/**
 * @brief Remapping scales geometry between boxes.
 */
static void test_remap(void)
{
    Element rectangle = make_rectangle(0, 0, 100, 100);
    FloatRect oldBox = {0, 0, 100, 100};
    FloatRect newBox = {0, 0, 200, 50};
    element_remap(&rectangle, oldBox, newBox);
    assert(rectangle.to.x == 200 && rectangle.to.y == 50);
}

int main(void)
{
    test_add_undo_redo();
    test_commit_clears_redo();
    test_clear_is_undoable();
    test_record_change();
    test_grouped_remove();
    test_pen_copy();
    test_hit_rectangle();
    test_remap();
    puts("scene tests passed");
    return 0;
}
