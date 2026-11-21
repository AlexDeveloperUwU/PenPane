#include "geometry.h"

#include <math.h>

void dirty_reset(Dirty *dirty)
{
    dirty->empty = true;
    dirty->rect = (RECT){0, 0, 0, 0};
}

void dirty_add(Dirty *dirty, RECT rect)
{
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }
    if (dirty->empty) {
        dirty->rect = rect;
        dirty->empty = false;
        return;
    }
    dirty->rect.left = dirty->rect.left < rect.left ? dirty->rect.left : rect.left;
    dirty->rect.top = dirty->rect.top < rect.top ? dirty->rect.top : rect.top;
    dirty->rect.right = dirty->rect.right > rect.right ? dirty->rect.right : rect.right;
    dirty->rect.bottom = dirty->rect.bottom > rect.bottom ? dirty->rect.bottom : rect.bottom;
}

bool rect_intersect(RECT a, RECT b, RECT *result)
{
    RECT out;
    out.left = a.left > b.left ? a.left : b.left;
    out.top = a.top > b.top ? a.top : b.top;
    out.right = a.right < b.right ? a.right : b.right;
    out.bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
    if (out.right <= out.left || out.bottom <= out.top) {
        return false;
    }
    *result = out;
    return true;
}

RECT rect_inflate(RECT rect, int amount)
{
    rect.left -= amount;
    rect.top -= amount;
    rect.right += amount;
    rect.bottom += amount;
    return rect;
}

RECT rect_from_float(FloatRect box, float margin)
{
    RECT rect;
    rect.left = (LONG)floorf(box.left - margin);
    rect.top = (LONG)floorf(box.top - margin);
    rect.right = (LONG)ceilf(box.right + margin);
    rect.bottom = (LONG)ceilf(box.bottom + margin);
    return rect;
}

FloatRect float_rect_from_points(Point a, Point b)
{
    FloatRect box;
    box.left = a.x < b.x ? a.x : b.x;
    box.right = a.x > b.x ? a.x : b.x;
    box.top = a.y < b.y ? a.y : b.y;
    box.bottom = a.y > b.y ? a.y : b.y;
    return box;
}

float segment_distance(Point point, Point from, Point to)
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float lengthSquared = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSquared > 0.0f) {
        t = ((point.x - from.x) * dx + (point.y - from.y) * dy) / lengthSquared;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    float px = from.x + dx * t - point.x;
    float py = from.y + dy * t - point.y;
    return sqrtf(px * px + py * py);
}
