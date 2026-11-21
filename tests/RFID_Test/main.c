#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "RC522.h"

static const uint16_t targets[] = {750, 1000, 500};

/* Exact command syntax requires the displayed UID; target values are only
 * provisioning inputs. Runtime targets are always decoded from card memory. */
static bool parse_set(const char *line, unsigned *card, RfidUid *uid) {
    if (strlen(line) != 14 || memcmp(line, "SET ", 4) != 0 ||
        line[4] < '1' || line[4] > '3' || line[5] != ' ') { return false; }
    *card = (unsigned)(line[4] - '1');
    *uid = (RfidUid){.length = 4};
    for (unsigned i = 0; i < 8; ++i) {
        char ch = line[6 + i];
        unsigned nibble;
        if (ch >= '0' && ch <= '9') { nibble = (unsigned)(ch - '0'); }
        else if (ch >= 'A' && ch <= 'F') { nibble = (unsigned)(ch - 'A' + 10); }
        else if (ch >= 'a' && ch <= 'f') { nibble = (unsigned)(ch - 'a' + 10); }
        else { return false; }
        uid->uid[i / 2] = (uint16_t)((uid->uid[i / 2] << 4) | nibble);
    }
    return true;
}

static void provision(const char *line, const RfidUid *last, bool have_card) {
    RfidUid requested; unsigned card;
    if (!parse_set(line, &card, &requested)) {
        puts("Invalid command. Use SET <1|2|3> <8 hex UID digits>, then Enter.");
        return;
    }
    if (!have_card || last->length != 4) { puts("Scan a supported card first."); return; }
    for (unsigned i = 0; i < 4; ++i) {
        if (requested.uid[i] != last->uid[i]) { puts("UID differs from the last scanned card. No write."); return; }
    }
    printf("Provisioning Card %u: target=%u raw ADC counts. Keep this card in range.\n",
           card + 1, (unsigned)targets[card]);
    AppStatus status = rc522_provision_target_level(&requested, targets[card]);
    if (status != APP_OK) {
        printf("Provision failed (status=%d). Nonblank/different records are refused; key/access or communication may also fail.\n", status);
        puts("A failed write may leave unverified data. No automatic retry; inspect block 4.");
        return;
    }
    uint16_t actual = 0;
    status = rc522_read_target_level(&requested, &actual);
    if (status == APP_OK && actual == targets[card]) {
        printf("VERIFIED Card %u: target=%u read from card memory. Remove card and label it.\n",
               card + 1, (unsigned)actual);
    } else {
        printf("Separate target readback failed (status=%d). Rescan to verify.\n", status);
    }
}

int main(void) {
    stdio_init_all();
    // Connect a USB serial monitor before starting the demonstration.
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(500);
    if (rc522_init() != APP_OK) {
        printf("RC522 initialization failed. Check 3.3V, wiring, CS and reset.\n");
        return 1;
    }
    puts("RFID target setup: normal scans READ ONLY. Writes require an explicit SET command.");
    puts("Card 1 = 750; Card 2 = 1000; Card 3 = 500 raw ADC counts.");
    puts("Scan ONE card and keep it in range. Type SET 1 XXXXXXXX (displayed UID, no spaces), then Enter.");
    puts("Use SET 2 or SET 3 for the other targets. Only blank block 4 is written; no key/trailer changes.");
    RfidUid last_uid = {0}; bool have_card = false;
    char command[32]; unsigned command_length = 0; bool overflow = false;

    while (true) {
        RfidUid uid;
        AppStatus status = rc522_poll(&uid);
        if (status == APP_OK) {
            last_uid = uid; have_card = true;
            printf("UID (%u bytes):", (unsigned)uid.length);
            for (uint8_t i = 0; i < uid.length; ++i) {
                printf(" %02X", (unsigned)uid.uid[i]);
            }
            printf("\n");
            RfidCardInfo info;
            if (rc522_get_card_info(&info) != APP_OK) {
                printf("RFID metadata unavailable; remove the card and retry.\n");
            } else {
                printf("ATQA bytes (received order): %02X %02X\n",
                       (unsigned)info.atqa[0], (unsigned)info.atqa[1]);
                printf("SAK: %02X\n", (unsigned)info.sak);
                printf("Detected PICC family (SAK): %s\n", info.type_name);
                printf("Library data-reading capability: %s\n", info.memory_read_support);
                if ((info.sak & 0x7f) == 0) {
                    printf("SAK 00 is shared by Ultralight/NTAG variants; exact IC is not established.\n");
                }
                printf("Family classification is not proof of exact silicon or successful memory access.\n\n");
                if (uid.length == 4 && (info.sak & 0x7f) == 0x08) {
                    uint8_t data[16];
                    status = rc522_read_block4(&uid, data);
                    if (status == APP_OK) {
                        printf("Block 4 HEX:");
                        for (unsigned i = 0; i < sizeof data; ++i) { printf(" %02X", (unsigned)data[i]); }
                        printf("\nBlock 4 ASCII: |");
                        for (unsigned i = 0; i < sizeof data; ++i) {
                            putchar(data[i] >= 32 && data[i] <= 126 ? data[i] : '.');
                        }
                        bool zero = true, ff = true;
                        for (unsigned i = 0; i < sizeof data; ++i) {
                            zero = zero && data[i] == 0;
                            ff = ff && data[i] == 0xff;
                        }
                        printf("|\n");
                        if (zero || ff) { printf("Uniform %s block; no target record established.\n", zero ? "00" : "FF"); }
                        else {
                            uint16_t actual;
                            status = rc522_read_target_level(&uid, &actual);
                            if (status == APP_OK) { printf("TARGET FROM CARD: %u raw ADC counts\n", (unsigned)actual); }
                            else { printf("No verified target record (status=%d). No value inferred.\n", status); }
                        }
                    } else {
                        printf("Block 4 read failed (AppStatus=%d): factory key may differ, access may be denied, or communication failed.\n", status);
                    }
                } else {
                    printf("Block read skipped: requires 4-byte UID and SAK 08.\n");
                }
                printf("Keep card in range to SET, or remove/re-tap to read again.\n\n");
            }
        } else if (status != APP_NOT_READY) {
            have_card = false;
            printf("RFID error: check reader or remove the card and retry.\n");
            sleep_ms(900);
        }
        if (!stdio_usb_connected()) {
            command_length = 0; overflow = false; have_card = false;
        } else for (unsigned count = 0; count < 64; ++count) {
            int ch = getchar_timeout_us(0);
            if (ch < 0) { break; }
            if (ch == '\r' || ch == '\n') {
                if (overflow) { puts("Command too long; discarded. No write."); }
                else if (command_length) {
                    command[command_length] = 0;
                    provision(command, &last_uid, have_card);
                }
                command_length = 0; overflow = false;
            } else if (command_length + 1 < sizeof command) {
                /* NUL/control characters invalidate the entire line. */
                if (ch < 32 || ch > 126) { overflow = true; }
                command[command_length++] = (char)ch;
            } else { overflow = true; }
        }
        sleep_ms(100);
    }
}
