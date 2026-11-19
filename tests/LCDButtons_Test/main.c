#include <stdio.h>
#include "LCD1602.h"
#include "Buttons.h"
#include "AppConfig.h"

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

static void show_press(const char *message) {
    printf("%s\n", message);
    if (lcd1602_show_text(message) != APP_OK) {
        printf("FAIL: LCD text write.\n");
    }
}

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(1000);
    printf("LCD test: SDA GP%d, SCL GP%d, I2C address 0x27.\n",
           PIN_LCD_SDA, PIN_LCD_SCL);

    if (lcd1602_init() != APP_OK) {
        printf("FAIL: LCD initialization. Check power, level shifting, wiring and address.\n");
        return 1;
    }
    if (lcd1602_show_text("LCD test OK") != APP_OK) {
        printf("FAIL: LCD text write.\n");
        return 1;
    }
    if (buttons_init() != APP_OK) {
        printf("FAIL: Button initialization.\n");
        return 1;
    }
    printf("LCD ready. Submit GP%d, Reset GP%d: press a button.\n",
           PIN_SUBMIT_BUTTON, PIN_RESET_BUTTON);
    printf("If the screen is blank, adjust the backpack contrast potentiometer.\n");
    ButtonEvents previous = {0};
    while (true) {
        ButtonEvents first, second;
        if (buttons_poll(&first) != APP_OK) { return 1; }
        sleep_ms(20);
        if (buttons_poll(&second) != APP_OK) { return 1; }

        // Accept two matching samples; report only a new press, not a held key.
        if (first.submit_pressed == second.submit_pressed) {
            if (second.submit_pressed && !previous.submit_pressed) {
                show_press("Submit pressed");
            }
            previous.submit_pressed = second.submit_pressed;
        }
        if (first.reset_pressed == second.reset_pressed) {
            if (second.reset_pressed && !previous.reset_pressed) {
                show_press("Reset pressed");
            }
            previous.reset_pressed = second.reset_pressed;
        }
        sleep_ms(20);
    }
}
#else
int main(void) {
    printf("The LCD hardware test requires a Pico SDK build.\n");
    return 0;
}
#endif
