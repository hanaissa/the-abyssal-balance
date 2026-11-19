#ifndef ABYSSAL_POTENTIOMETER_H
#define ABYSSAL_POTENTIOMETER_H

#include "AppTypes.h"

AppStatus potentiometer_init(void);

AppStatus potentiometer_read_percent(uint8_t *percent);

AppStatus potentiometer_command(PumpCommand *command);

#endif
