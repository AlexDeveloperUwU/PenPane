/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/controller.h"
#include "../src/trail.h"

#define CANVAS_WIDTH 400
#define CANVAS_HEIGHT 200

/**
 * @brief Renders a trail on a cleared canvas and sums the alpha of every pixel.
 * @param trail Trail to draw.
 * @param canvas Scratch canvas.
 * @param nowMs Clock used for fading.
 * @return Total alpha, a measure of how much ink is visible.
 */
static double ink_at(const Trail *trail, Canvas *canvas, uint32_t nowMs)
{
    memset(canvas->pixels, 0, (size_t)canvas->width * (size_t)canvas->height * sizeof(uint32_t));
    RECT clip = {0, 0, canvas->width, canvas->height};
    trail_render(trail, canvas, &clip, nowMs);
    double total = 0.0;
    for (int i = 0; i < canvas->width * canvas->height; i++) {
        total += (double)(canvas->pixels[i] >> 24);
    }
    return total;
}

/**
 * @brief Builds a laser trail from a short quick stroke.
 * @param trail Trail to fill.
 * @param points Number of samples.
 * @param length Stroke length in pixels.
 * @param stepMs Time between samples.
 * @return Time of the last sample.
 */
static uint32_t short_stroke(Trail *trail, int points, float length, uint32_t stepMs)
{
    trail_init(trail, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, 0xFFFF0000u, true);
    uint32_t now = 1000;
    for (int i = 0; i < points; i++) {
        Point p = {100.0f + length * (float)i / (float)(points - 1), 100.0f};
        trail_add(trail, p, now);
        now += stepMs;
    }
    return now - stepMs;
}

/**
 * @brief A short stroke must fade visibly and steadily, not sit unchanged and then vanish at the end.
 */
static void test_short_stroke_fades_steadily(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    Trail trail;
    uint32_t last = short_stroke(&trail, 6, 40.0f, 16);
    double start = ink_at(&trail, &canvas, last);
    double quarter = ink_at(&trail, &canvas, last + 250);
    double half = ink_at(&trail, &canvas, last + 500);
    double late = ink_at(&trail, &canvas, last + 800);
    printf("short stroke ink: start %.0f  +250ms %.0f  +500ms %.0f  +800ms %.0f\n", start, quarter, half,
           late);
    assert(start > 0.0);
    assert(quarter < start * 0.85);
    assert(half < quarter * 0.85);
    assert(late < half * 0.85);
    canvas_destroy(&canvas);
}

/**
 * @brief Alpha at a pixel of a rendered trail.
 * @param canvas Canvas rendered into.
 * @param x Pixel column.
 * @return Alpha 0 to 255 on the row of the test strokes.
 */
static unsigned alpha_at(const Canvas *canvas, int x)
{
    return canvas->pixels[100 * canvas->width + x] >> 24;
}

/**
 * @brief A short quick stroke is wiped from its first point to its last once released, and then it is gone.
 */
static void test_released_stroke_sweeps_tail_to_head(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    Trail trail;
    uint32_t last = short_stroke(&trail, 6, 60.0f, 8);
    trail_settle(&trail, last);
    trail_release(&trail);
    ink_at(&trail, &canvas, last);
    assert(alpha_at(&canvas, 102) > 0 && alpha_at(&canvas, 158) > 0);
    unsigned tailMid = 0;
    unsigned headMid = 0;
    for (uint32_t t = last; t < last + 900; t += 16) {
        ink_at(&trail, &canvas, t);
        tailMid = alpha_at(&canvas, 106);
        headMid = alpha_at(&canvas, 150);
        if (tailMid == 0) {
            break;
        }
    }
    printf("sweep: tail gone while head alpha is %u\n", headMid);
    assert(tailMid == 0 && headMid > 0);
    assert(!trail_prune(&trail, last + 1200));
    canvas_destroy(&canvas);
}

/**
 * @brief Whatever the stroke's length or speed, the wipe starts visible, only ever loses ink, and ends empty.
 */
static void test_release_is_independent_of_speed_and_length(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    const int counts[] = {1, 2, 3, 6, 40};
    const float steps[] = {0.0f, 2.0f, 6.0f};
    const uint32_t gaps[] = {1, 8, 120};
    for (int c = 0; c < 5; c++) {
        for (int s = 0; s < 3; s++) {
            for (int g = 0; g < 3; g++) {
                Trail trail;
                trail_init(&trail, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, 0xFFFF0000u, true);
                trail_begin(&trail);
                uint32_t now = 5000;
                for (int i = 0; i < counts[c]; i++) {
                    Point p = {20.0f + steps[s] * (float)i, 100.0f};
                    trail_add(&trail, p, now);
                    now += gaps[g];
                }
                trail_settle(&trail, now);
                trail_release(&trail);
                double previous = ink_at(&trail, &canvas, now);
                assert(previous > 0.0);
                for (uint32_t t = now + 16; t < now + 1100; t += 16) {
                    double ink = ink_at(&trail, &canvas, t);
                    assert(ink <= previous + 1.0);
                    previous = ink;
                }
                assert(!trail_prune(&trail, now + 1100));
            }
        }
    }
    canvas_destroy(&canvas);
}

/**
 * @brief A tap with the laser draws a dot that fades, instead of nothing.
 */
static void test_single_point_is_visible(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    Trail trail;
    trail_init(&trail, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, 0xFFFF0000u, true);
    Point p = {100.0f, 100.0f};
    trail_add(&trail, p, 1000);
    assert(ink_at(&trail, &canvas, 1000) > 0.0);
    assert(ink_at(&trail, &canvas, 1500) < ink_at(&trail, &canvas, 1000));
    canvas_destroy(&canvas);
}

/**
 * @brief The last laser point reaches the release position despite streamlining.
 */
static void test_laser_reaches_release_position(void)
{
    Controller controller;
    Scene scene;
    Canvas canvas;
    scene_init(&scene);
    controller_init(&controller);
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    controller_attach(&controller, &scene, &canvas);
    controller_set_tool(&controller, TOOL_LASER);
    uint32_t clock = 1000;
    PointerSample down = {{50, 50}, -1.0f, clock, false};
    controller_pointer_down(&controller, &down);
    for (int i = 1; i <= 4; i++) {
        clock += 8;
        PointerSample move = {{50.0f + 20.0f * (float)i, 50.0f}, -1.0f, clock, false};
        controller_pointer_move(&controller, &move, true);
    }
    clock += 8;
    PointerSample up = {{130, 50}, -1.0f, clock, false};
    controller_pointer_up(&controller, &up);
    const Trail *laser = &controller.laser;
    Point last = laser->points[(laser->head + TRAIL_CAPACITY - 1) % TRAIL_CAPACITY].position;
    printf("laser last point %.1f (release at 130)\n", last.x);
    assert(fabsf(last.x - 130.0f) < 1.0f);
    controller_destroy(&controller);
    scene_destroy(&scene);
    canvas_destroy(&canvas);
}

/**
 * @brief Lifting the pen with the eraser leaves no trail behind.
 */
static void test_eraser_trail_gone_on_release(void)
{
    Controller controller;
    Scene scene;
    Canvas canvas;
    scene_init(&scene);
    controller_init(&controller);
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    controller_attach(&controller, &scene, &canvas);
    controller_set_tool(&controller, TOOL_ERASER);
    PointerSample down = {{50, 50}, -1.0f, 1000, false};
    controller_pointer_down(&controller, &down);
    PointerSample move = {{120, 60}, -1.0f, 1016, false};
    controller_pointer_move(&controller, &move, true);
    assert(controller.eraser.count > 0);
    PointerSample up = {{120, 60}, -1.0f, 1032, false};
    controller_pointer_up(&controller, &up);
    assert(controller.eraser.count == 0);
    Dirty repaint;
    Dirty direct;
    controller_take_dirty(&controller, &repaint, &direct);
    assert(!repaint.empty);
    controller_destroy(&controller);
    scene_destroy(&scene);
    canvas_destroy(&canvas);
}

/**
 * @brief In pointer mode the laser shows a dot that follows hovering and leaves no trail.
 */
static void test_laser_pointer_dot_follows_hover(void)
{
    Controller controller;
    Scene scene;
    Canvas canvas;
    scene_init(&scene);
    controller_init(&controller);
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    controller_attach(&controller, &scene, &canvas);
    controller_set_tool(&controller, TOOL_LASER);
    controller.laserPointer = true;
    PointerSample hover = {{100, 80}, -1.0f, 1000, false};
    controller_pointer_move(&controller, &hover, false);
    assert(controller.laserDotVisible && controller.laser.count == 0);
    PointerSample down = {{120, 90}, -1.0f, 1016, false};
    controller_pointer_down(&controller, &down);
    PointerSample drag = {{160, 90}, -1.0f, 1032, false};
    controller_pointer_move(&controller, &drag, true);
    assert(controller.laser.count == 0 && fabsf(controller.laserDot.x - 160.0f) < 0.1f);
    memset(canvas.pixels, 0, (size_t)CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(uint32_t));
    RECT clip = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
    controller_draw_overlays(&controller, &canvas, &clip, 1032);
    assert((canvas.pixels[90 * CANVAS_WIDTH + 160] >> 24) > 200);
    controller_cancel(&controller);
    assert(!controller.laserDotVisible);
    controller_destroy(&controller);
    scene_destroy(&scene);
    canvas_destroy(&canvas);
}

/**
 * @brief Two laser strokes in different places are not joined by a line.
 */
static void test_separate_strokes_are_not_joined(void)
{
    Canvas canvas;
    assert(canvas_create(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT));
    Trail trail;
    trail_init(&trail, TRAIL_LASER_RADIUS, TRAIL_LASER_LIFETIME_MS, 0xFFFF0000u, true);
    uint32_t now = 1000;
    for (int i = 0; i < 5; i++, now += 16) {
        trail_add(&trail, (Point){50.0f + 10.0f * (float)i, 100.0f}, now);
    }
    trail_begin(&trail);
    for (int i = 0; i < 5; i++, now += 16) {
        trail_add(&trail, (Point){300.0f + 10.0f * (float)i, 100.0f}, now);
    }
    ink_at(&trail, &canvas, now);
    for (int x = 130; x < 280; x++) {
        assert((canvas.pixels[100 * CANVAS_WIDTH + x] >> 24) == 0);
    }
    assert((canvas.pixels[100 * CANVAS_WIDTH + 320] >> 24) > 0);
    canvas_destroy(&canvas);
}

int main(void)
{
    test_separate_strokes_are_not_joined();
    test_short_stroke_fades_steadily();
    test_released_stroke_sweeps_tail_to_head();
    test_release_is_independent_of_speed_and_length();
    test_single_point_is_visible();
    test_laser_reaches_release_position();
    test_eraser_trail_gone_on_release();
    test_laser_pointer_dot_follows_hover();
    printf("trail tests passed\n");
    return 0;
}
