#include <stdio.h>
#include "pico/stdlib.h"
#include "WaterLevel.h"
#include "LCD1602.h"
#include "RC522.h"

enum { SAMPLE_MS = 500, CHANGE_COUNTS = 1, TARGET_TOLERANCE = 3 };

/* Keep USB available after a fault: returning from main can stop enumeration
 * before a terminal connects, hiding the initialization diagnostic. */
static int stop_with_diagnostics(const char *message) {
    for (;;) {
        puts(message);
        sleep_ms(1000);
    }
    return 1;
}

/* The entry point only prompts/dispatches; capture, validation, filtering and
 * conversion are production WaterLevel APIs. No buttons/pins are reassigned. */
static bool calibration_input(unsigned *stage, bool *ready) {
    static WaterLevelCalibrationPoint points[4];
    static const char keys[] = {'0', '5', '7', '1'};
    static const char *names[] = {"Dry", "50%", "75%", "Full"};
    int key = getchar_timeout_us(0);
    if (key == '0') {
        water_level_clear_calibration();
        *stage = 0;
        *ready = false;
    }
    if (*ready || *stage >= 4 || key != keys[*stage]) { return key == '0'; }
    if (lcd1602_show_lines("Sampling...", "Hold depth still") != APP_OK) {
        stop_with_diagnostics("FAIL: LCD calibration prompt.");
    }
    AppStatus status = water_level_capture_calibration_point(&points[*stage]);
    if (status != APP_OK) {
        printf("Calibration capture failed: %d. Start again with 0.\n", status);
        *stage = 4;
        return true;
    }
    printf("CAL %s: min=%u center=%u max=%u\n", names[*stage],
           (unsigned)points[*stage].minimum, (unsigned)points[*stage].center,
           (unsigned)points[*stage].maximum);
    ++*stage;
    if (*stage == 4) {
        status = water_level_set_calibration(points);
        if (status != APP_OK) {
            puts("CAL REJECTED: overlapping/touching or invalid ranges. No adjustment applied; ask owner before proceeding.");
        } else {
            *ready = true;
            puts("CAL ACCEPTED in RAM. Tap a card. Recalibration required after reboot.");
            if (lcd1602_show_lines("Calibration OK", "Tap target card") != APP_OK) {
                stop_with_diagnostics("FAIL: LCD calibration result.");
            }
        }
    }
    return true;
}

int main(void) {
    stdio_init_all();
    /* Run without a USB terminal using production drivers. */
    AppStatus sensor_status = water_level_init();
    if (lcd1602_init() != APP_OK) {
        puts("FAIL: LCD initialization; check power, bus levels and wiring.");
        return stop_with_diagnostics("FAIL: LCD initialization; check LCD supply, GP10/GP11, address 0x27 and bus levels.");
    }
    if (sensor_status != APP_OK) {
        puts("FAIL: Water sensor initialization.");
        if (lcd1602_show_text("Sensor init err") != APP_OK) {
            puts("FAIL: LCD write.");
        }
        return stop_with_diagnostics("FAIL: Water sensor initialization; restart after checking wiring.");
    }

    if (rc522_init() != APP_OK) {
        puts("FAIL: RFID initialization.");
        if (lcd1602_show_text("RFID init err") != APP_OK) {
            puts("FAIL: LCD write.");
        }
        return stop_with_diagnostics("FAIL: RFID initialization; check reader power and wiring, then restart.");
    }
    if (lcd1602_show_lines(water_level_is_calibrated() ? "Tap target card" : "Calibrate first",
                          water_level_is_calibrated() ? "Scale: 0..1000" : "USB: 0,5,7,1") != APP_OK) {
        puts("FAIL: LCD write.");
        return stop_with_diagnostics("FAIL: LCD write of startup prompt; check LCD wiring and restart.");
    }

    uint16_t displayed = 0;
    uint16_t target = 0;
    bool have_target = false;
    bool target_changed = false;
    bool have_reading = false;
    bool showing_error = false;
    AppStatus previous_card_status = APP_NOT_READY;
    unsigned calibration_stage = 0;
    bool calibration_ready = water_level_is_calibrated();
    puts("Fixed ADC anchors: 1850=50%, 2000=75%, 2100=full. Tap a target card.");
    puts("Optional fresh calibration: 0=dry, 5=50%, 7=75%, 1=full, in order. Reboot restores fixed anchors.");
    for (;;) {
        if (calibration_input(&calibration_stage, &calibration_ready)) {
            have_reading = false;
            showing_error = false;
            have_target = false;
        }
        if (!calibration_ready) {
            static const char *prompts[] = {"Dry: send 0", "50%: send 5", "75%: send 7", "Full: send 1", "Cal failed"};
            uint16_t ignored, raw;
            (void)water_level_read(&ignored);
            char second[17];
            if (water_level_get_last_raw(&raw) == APP_OK) {
                snprintf(second, sizeof second, "Raw ADC: %u", (unsigned)raw);
                printf("UNCALIBRATED raw=%u; %s\n", (unsigned)raw, prompts[calibration_stage]);
            } else {
                snprintf(second, sizeof second, "Sensor read err");
            }
            if (lcd1602_show_lines(prompts[calibration_stage], second) != APP_OK) {
                stop_with_diagnostics("FAIL: LCD raw/calibration display.");
            }
            sleep_ms(SAMPLE_MS);
            continue;
        }
        RfidUid uid;
        AppStatus card_status = rc522_poll(&uid);
        if (card_status == APP_OK) {
            uint16_t card_target;
            card_status = rc522_read_target_level(&uid, &card_target);
            /* This owner-requested demo uses the card's stored numeric value
             * as a 0..1000 target. It never changes the legacy card record. */
            if (card_status == APP_OK && card_target <= 1000u) {
                target = card_target;
                have_target = true;
                target_changed = true;
                printf("Target from card: %u on calibrated 0..1000 demo scale.\n", (unsigned)target);
            } else {
                /* A newly presented invalid card must not reuse the old target. */
                have_target = false;
                card_status = APP_ERROR;
            }
        }
        if (card_status != APP_OK && card_status != APP_NOT_READY) {
            have_target = false;
            if (card_status != previous_card_status) {
                puts("FAIL: RFID/target read; remove and retap a valid target card.");
                if (lcd1602_show_text("RFID/target err") != APP_OK) {
                    puts("FAIL: LCD write; demo stopped.");
                    return stop_with_diagnostics("FAIL: LCD write of RFID error; check LCD wiring and restart.");
                }
            }
        }
        previous_card_status = card_status;

        uint16_t raw;
        uint16_t current;
        AppStatus status = water_level_read(&current);
        if (status == APP_OK) { status = water_level_get_last_raw(&raw); }
        if (status != APP_OK || raw > 4095u) {
            if (!showing_error) {
                puts("FAIL: Water sensor read; retrying.");
                if (lcd1602_show_text("Sensor read err") != APP_OK) {
                    puts("FAIL: LCD write; demo stopped.");
                    return stop_with_diagnostics("FAIL: LCD write of sensor error; check LCD wiring and restart.");
                }
            }
            showing_error = true;
            have_reading = false;
        } else {
            int change = (int)current - (int)displayed;
            if (have_target && (!have_reading || showing_error || target_changed ||
                change >= CHANGE_COUNTS || change <= -CHANGE_COUNTS)) {
                if (lcd1602_show_levels(target, current) != APP_OK) {
                    puts("FAIL: LCD write; demo stopped.");
                    return stop_with_diagnostics("FAIL: LCD write of levels; check LCD wiring and restart.");
                }
                bool matched = water_level_within_tolerance(current, target, TARGET_TOLERANCE);
                if (matched) {
                    char text[17];
                    snprintf(text, sizeof text, "Target: %u OK", (unsigned)target);
                    if (lcd1602_show_text(text) != APP_OK) {
                        puts("FAIL: LCD write; demo stopped.");
                        return stop_with_diagnostics("FAIL: LCD write of tolerance indicator; check LCD wiring and restart.");
                    }
                }
                printf("Target: %u | Current: %u | Raw ADC: %u | Within +/-3: %s\n",
                       (unsigned)target, (unsigned)current, (unsigned)raw, matched ? "yes" : "no");
                displayed = current;
                have_reading = true;
                showing_error = false;
                target_changed = false;
            }
        }
        sleep_ms(SAMPLE_MS);
    }
}

