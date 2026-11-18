#include "LCD1602.h"
AppStatus lcd1602_init(void) { return APP_NOT_READY; }
AppStatus lcd1602_show_levels(uint16_t target, uint16_t current) { (void)target; (void)current; return APP_NOT_READY; }
AppStatus lcd1602_show_text(const char *text) { (void)text; return APP_NOT_READY; }
