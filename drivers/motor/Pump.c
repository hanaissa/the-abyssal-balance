#include "Pump.h"
#include "L298N.h"
AppStatus pump_init(void) { l298n_emergency_stop(); return l298n_init(); }
AppStatus pump_apply(PumpCommand command) { return l298n_apply(command.direction, command.speed_percent); }
void pump_stop(void) { l298n_emergency_stop(); }
