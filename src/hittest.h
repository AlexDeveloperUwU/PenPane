/**
 * @file hittest.h
 * @brief Pointer hit testing against elements and selection handles.
 */
#ifndef INK_HITTEST_H
#define INK_HITTEST_H

#include <stdbool.h>

#include "element.h"

#define HIT_HANDLE_NONE (-1)
#define HIT_HANDLE_COUNT 8

/**
 * @brief Tests whether a point touches an element's stroke or filled area.
 * @param element Element to test.
 * @param point Query point.
 * @param tolerance Extra distance in pixels that still counts as a hit.
 * @return true on a hit.
 */
bool hit_element(const Element *element, Point point, float tolerance);

/**
 * @brief Finds which resize handle of a selected element is under a point.
 * @param element Selected element.
 * @param point Query point.
 * @return Handle index 0..7 (clockwise from top-left: TL, T, TR, R, BR, B, BL, L), or HIT_HANDLE_NONE.
 */
int hit_handle(const Element *element, Point point);

#endif
