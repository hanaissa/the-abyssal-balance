#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "UV_LED.h"

int main(void) {
    stdio_init_all();
    if (uv_led_init() != APP_OK) {
        printf("FAIL: UV LED initialization.\n");
        return 1;
    }
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(1000);
    printf("UV LED test on GP22, physical pin 29. Use a series resistor.\n");
    while (true) {
        if (uv_led_set(true) != APP_OK) {
            uv_led_set(false);
            printf("FAIL: UV LED control.\n");
            return 1;
        }
        printf("UV LED ON\n");
        sleep_ms(1000);
        if (uv_led_set(false) != APP_OK) {
            printf("FAIL: UV LED control.\n");
            return 1;
        }
        printf("UV LED OFF\n");
        sleep_ms(1000);
    }
}
