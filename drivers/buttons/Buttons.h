#ifndef ABYSSAL_BUTTONS_H
#define ABYSSAL_BUTTONS_H
#include "AppTypes.h"
AppStatus buttons_init(void);
AppStatus buttons_poll(ButtonEvents *events);
#endif
