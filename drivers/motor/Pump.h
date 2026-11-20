#ifndef ABYSSAL_PUMP_H
#define ABYSSAL_PUMP_H
#include "AppTypes.h"
AppStatus pump_init(void);
/* Separate fixed-direction pumps, never enabled together. Duty is 0..100%;
 * zero stops both. These functions do not change the hardwired IN1..IN4.
 * Call pump_init first and use one control loop. */
AppStatus pump_start_fill(uint8_t speed_percent);
AppStatus pump_start_drain(uint8_t speed_percent);
AppStatus pump_apply(PumpCommand command);
/* Shared stop for either pump; safe before initialization. */
void pump_stop(void);
#endif
