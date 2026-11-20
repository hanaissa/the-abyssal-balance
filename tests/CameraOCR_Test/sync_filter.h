#ifndef ABYSSAL_CAMERA_SYNC_FILTER_H
#define ABYSSAL_CAMERA_SYNC_FILTER_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { bool active; uint32_t since; } SyncFilter;
static inline bool sync_level_stable(SyncFilter *filter, bool level,
                                     uint32_t now, uint32_t minimum_us) {
    if (!level) { filter->active = false; return false; }
    if (!filter->active) { filter->active = true; filter->since = now; }
    return (uint32_t)(now - filter->since) >= minimum_us;
}
#endif
