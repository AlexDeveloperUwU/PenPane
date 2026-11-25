/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file hatch.h
 * @brief Hachure and cross-hatch fills applied to a shape's coverage mask.
 */
#ifndef PENPANE_HATCH_H
#define PENPANE_HATCH_H

#include "raster.h"
#include "style.h"

/**
 * @brief Cuts diagonal stripes out of a filled mask so only the hatch lines remain.
 *
 * The stripes are anchored at origin, so a shape keeps its pattern while it moves. Spacing and line
 * thickness follow the stroke width. A solid fill leaves the mask unchanged.
 *
 * @param mask Mask already holding the shape's filled coverage; modified in place.
 * @param origin Pattern anchor, normally the top-left corner of the shape.
 * @param fillStyle Fill style; FILL_STYLE_SOLID is a no-op.
 * @param strokeWidth Stroke width of the shape in pixels.
 */
void mask_apply_hatch(Mask *mask, Point origin, FillStyle fillStyle, float strokeWidth);

#endif
