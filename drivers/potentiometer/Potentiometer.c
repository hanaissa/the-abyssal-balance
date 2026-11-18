#include "Potentiometer.h"
AppStatus potentiometer_init(void) { return APP_NOT_READY; }
AppStatus potentiometer_read_percent(uint8_t *percent) { (void)percent; return APP_NOT_READY; }
AppStatus potentiometer_command(PumpCommand *command) { (void)command; return APP_NOT_READY; }
