#ifndef ABYSSAL_PUMP_H
#define ABYSSAL_PUMP_H
#include "AppTypes.h"
AppStatus pump_init(void);
AppStatus pump_apply(PumpCommand command);
void pump_stop(void);
#endif
