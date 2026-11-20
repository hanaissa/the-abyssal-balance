#include <stdio.h>
#include "../../drivers/potentiometer/Potentiometer.h"

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#endif

int main(void) {
    /* Exercise rejected pointers on both host and device. */
    if (potentiometer_read_percent(NULL) != APP_ERROR ||
        potentiometer_command(NULL) != APP_ERROR) {
        return 1;
    }
#ifdef PICO_ON_DEVICE
    stdio_init_all();
    while (!stdio_usb_connected()) { sleep_ms(100); }
    sleep_ms(1000);
    if (potentiometer_init() != APP_OK) {
        puts("FAIL: potentiometer initialization.");
        return 1;
    }
    puts("Potentiometer test: GP26 / ADC0. No pump outputs are driven.");
    puts("Turn to both ends and the middle. Middle 45..55% requests STOP.");
    puts("NULL pointer rejection checks passed.");
    while (true) {
        uint8_t percent;
        PumpCommand command;
        if (potentiometer_read_percent(&percent) != APP_OK ||
            potentiometer_command(&command) != APP_OK) {
            puts("FAIL: potentiometer read.");
            return 1;
        }
        const char *direction = command.direction == PUMP_FILL ? "FILL" :
                                command.direction == PUMP_DRAIN ? "DRAIN" : "STOP";
        printf("Knob: %3u%% | Request: %s %3u%%\n",
               (unsigned)percent, direction, (unsigned)command.speed_percent);
        sleep_ms(300);
    }
#else
    if (potentiometer_init() != APP_UNSUPPORTED) { return 1; }
    puts("Host pointer checks passed. Knob readings require the Pico firmware.");
    return 0;
#endif
}
