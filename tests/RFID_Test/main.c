#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "RC522.h"

int main(void) {
    stdio_init_all();
    // Connect a USB serial monitor before starting the demonstration.
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    if (rc522_init() != APP_OK) {
        printf("RC522 initialization failed. Check 3.3V, wiring, CS and reset.\n");
        return 1;
    }
    printf("Serial UID demo: tap one card, then remove it before tapping again.\n");

    while (true) {
        RfidUid uid;
        AppStatus status = rc522_poll(&uid);
        if (status == APP_OK) {
            printf("UID (%u bytes):", (unsigned)uid.length);
            for (uint8_t i = 0; i < uid.length; ++i) {
                printf(" %02X", (unsigned)uid.uid[i]);
            }
            printf("\n");
        } else if (status != APP_NOT_READY) {
            printf("RFID error: check reader or remove the card and retry.\n");
            sleep_ms(900);
        }
        sleep_ms(100);
    }
}
