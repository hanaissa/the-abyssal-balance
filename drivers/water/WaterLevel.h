#ifndef ABYSSAL_WATER_LEVEL_H
#define ABYSSAL_WATER_LEVEL_H
#include "AppTypes.h"
AppStatus water_level_init(void);
AppStatus water_level_read(uint16_t *level);
bool water_level_within_tolerance(uint16_t level, uint16_t target, uint16_t tolerance);
#endif
