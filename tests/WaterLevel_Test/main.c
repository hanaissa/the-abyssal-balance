#include <stdio.h>
#include "WaterLevel.h"

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(1000);
    if (water_level_init() != APP_OK) {
        printf("Water sensor initialization failed.\n");
        return 1;
    }
    printf("Water sensor: GP27 / ADC1, raw readings 0..4095.\n");
    printf("Compare dry and wet readings; no water-height calibration yet.\n");
    while (true) {
        uint16_t raw;
        if (water_level_read(&raw) == APP_OK) {
            unsigned millivolts = (unsigned)raw * 3300u / 4096u;
            printf("Raw: %4u / 4095 | Approx: %4u mV\n",
                   (unsigned)raw, millivolts);
        } else {
            printf("Water sensor read failed.\n");
        }
        sleep_ms(500);
    }
}
#else
int main(void) {
    printf("The water sensor hardware demo requires a Pico SDK build.\n");
    return 0;
}
#endif
