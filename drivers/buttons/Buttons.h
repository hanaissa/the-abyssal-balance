#ifndef ABYSSAL_BUTTONS_H
#define ABYSSAL_BUTTONS_H

#include "../../include/AppTypes.h"

AppStatus buttons_init(void);

AppStatus buttons_poll(ButtonEvents *events);

/* Two matching samples 20 ms apart, reporting only new presses. Blocks for
 * 20 ms; call from one control loop. Raw buttons_poll remains unchanged.
 * Call buttons_init first. NULL -> APP_ERROR; host -> APP_UNSUPPORTED. */
AppStatus buttons_poll_debounced(ButtonEvents *events);

#endif
