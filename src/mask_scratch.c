#include "mask_scratch.h"

#include <stdlib.h>
#include <string.h>

static uint8_t *scratchBuffer;
static size_t scratchCapacity;

bool mask_scratch_acquire(Mask *mask, RECT area)
{
    int width = area.right - area.left;
    int height = area.bottom - area.top;
    if (width <= 0 || height <= 0) {
        return false;
    }
    size_t needed = (size_t)width * (size_t)height;
    if (needed > scratchCapacity) {
        uint8_t *grown = realloc(scratchBuffer, needed);
        if (!grown) {
            return false;
        }
        scratchBuffer = grown;
        scratchCapacity = needed;
    }
    memset(scratchBuffer, 0, needed);
    mask->x = area.left;
    mask->y = area.top;
    mask->width = width;
    mask->height = height;
    mask->coverage = scratchBuffer;
    return true;
}

void mask_scratch_release(void)
{
    free(scratchBuffer);
    scratchBuffer = NULL;
    scratchCapacity = 0;
}

void mask_scratch_trim(size_t keepBytes)
{
    if (scratchCapacity > keepBytes) {
        mask_scratch_release();
    }
}
