
#ifndef ABYSSAL_LCD1602_H
#define ABYSSAL_LCD1602_H

#include <stdint.h>
#include "AppTypes.h"

AppStatus lcd1602_init(void);
AppStatus lcd1602_show_levels(uint16_t target, uint16_t current);
AppStatus lcd1602_show_text(const char *text);
/* Write both rows, padding/truncating each to 16 characters. */
AppStatus lcd1602_show_lines(const char *first, const char *second);

#endif
