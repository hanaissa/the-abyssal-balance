#include <stdio.h>
#include "LCD1602.h"

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(1000);
    printf("LCD test: SDA GP4, SCL GP5, I2C address 0x27.\n");

    if (lcd1602_init() != APP_OK) {
        printf("FAIL: LCD initialization. Check power, level shifting, wiring and address.\n");
        return 1;
    }
    if (lcd1602_show_text("LCD test OK") != APP_OK) {
        printf("FAIL: LCD text write.\n");
        return 1;
    }
    printf("LCD write completed. Look for 'LCD test OK' on the display.\n");
    printf("If the screen is blank, adjust the backpack contrast potentiometer.\n");
    while (true) {
        sleep_ms(1000);
    }
}
#else
int main(void) {
    printf("The LCD hardware test requires a Pico SDK build.\n");
    return 0;
}
#endif
