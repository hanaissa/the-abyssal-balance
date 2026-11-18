#include "L298N.h"
AppStatus l298n_init(void) { return APP_NOT_READY; }
AppStatus l298n_apply(PumpDirection direction, uint8_t speed_percent) { (void)direction; (void)speed_percent; return APP_NOT_READY; }
void l298n_emergency_stop(void) {}
