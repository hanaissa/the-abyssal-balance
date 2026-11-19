#ifndef ABYSSAL_L298N_H
#define ABYSSAL_L298N_H
#include "AppTypes.h"
/* Call the motor driver from a single control loop. Stop is safe before init. */
AppStatus l298n_init(void);
AppStatus l298n_apply(PumpDirection direction, uint8_t speed_percent);
void l298n_emergency_stop(void);
#endif
