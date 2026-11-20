#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "Pump.h"

enum { PULSE_MS = 500 };

int main(void)
{
    if (pump_init() != APP_OK) {
        pump_stop();
        return 1;
    }
    stdio_init_all();

    bool running = false;
    absolute_time_t stop_at = nil_time;
    bool announced = false;

    for (;;) {
        if (!stdio_usb_connected()) {
            if (running) {
                pump_stop();
                running = false;
            }
            announced = false;
            sleep_ms(10);
            continue;
        }
        if (!announced) {
            puts("Pump bench: f/d = fill/drain at 50%, F/D = 100%, s = stop.");
            puts("Each command gives a 500 ms test pulse. Start with motor supply off.");
            puts("GP8 = fill ENA, GP9 = drain ENB. External 12 V motor supply.");
            puts("Messages report software requests, not measured pump operation.");
            announced = true;
        }

        int key = getchar_timeout_us(1000);
        if (running && (key == 's' || time_reached(stop_at))) {
            pump_stop();
            running = false;
            puts(key == 's' ? "STOP requested; both enables off." :
                             "Pulse complete; both enables off.");
            continue;
        }
        if (running) {
            continue;
        }

        PumpDirection direction;
        switch (key) {
            case 'f':
            case 'F':
                direction = PUMP_FILL;
                break;
            case 'd':
            case 'D':
                direction = PUMP_DRAIN;
                break;
            case 's':
                pump_stop();
                puts("STOP requested; both enables off.");
                continue;
            default:
                continue;
        }

        uint8_t speed = (key == 'F' || key == 'D') ? 100u : 50u;
        /* Print before enabling: USB output may block if the host is slow. */
        printf("Request: %s %u%% for %u ms.\n",
               direction == PUMP_FILL ? "FILL" : "DRAIN",
               (unsigned)speed, (unsigned)PULSE_MS);
        stop_at = make_timeout_time_ms(PULSE_MS);
        AppStatus status = direction == PUMP_FILL ? pump_start_fill(speed) :
                                                   pump_start_drain(speed);
        if (status != APP_OK) {
            pump_stop();
            puts("Pump command failed. Both enables are off.");
            continue;
        }
        running = true;
    }
}
