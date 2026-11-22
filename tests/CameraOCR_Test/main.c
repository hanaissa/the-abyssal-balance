#include "StateMachine.h"

#ifdef PICO_ON_DEVICE
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "Camera.h"
#include "OCR.h"
#include "LCD1602.h"

/* Cache both visible rows, including padding, rather than the full OCR string. */
static AppStatus display_result(const char *text, bool captured, char displayed[33], bool *valid) {
    char rows[33];
    memset(rows, ' ', 32);
    rows[32] = '\0';
    if (captured) { memcpy(rows, "Code Captured:", 14); }
    size_t length = 0;
    while (length < 16 && text[length]) {
        rows[(captured ? 16 : 0) + length] = text[length];
        ++length;
    }
    if (*valid && memcmp(rows, displayed, sizeof rows) == 0) {
        return APP_OK;
    }
    char first[17];
    memcpy(first, rows, 16);
    first[16] = '\0';
    AppStatus status = lcd1602_show_lines(first, rows + 16);
    *valid = status == APP_OK;
    if (*valid) { memcpy(displayed, rows, sizeof rows); }
    return status;
}

static AppStatus capture_and_display(char displayed[33], bool *valid) {
    char text[256] = {0};
    AppStatus status = camera_capture();
    if (status == APP_OK) {
        status = ocr_read_text(text, sizeof text);
    } else {
        printf("Camera capture failed: %d\n", status);
    }
    /* OCR consumes the driver's current frame synchronously. Release it even
     * after a failed capture/read; never display a previous frame's text. */
    camera_release_frame();
    text[sizeof text - 1] = '\0';
    bool captured = status == APP_OK && text[0];
    const char *result = captured ? text : "Read failed";
    printf("OCR status=%d text=%s\n", status, result);
    return display_result(result, captured, displayed, valid);
}

static int stop_with_error(const char *message) {
    puts(message);
    if (lcd1602_show_text(message) != APP_OK) {
        puts("LCD error while displaying initialization failure.");
    }
    camera_deinit();
    return 1;
}
#endif

int main(void) {
    StateMachine machine;
    state_machine_init(&machine);
#ifdef PICO_ON_DEVICE
    /* Camera/LCD use the same existing I2C1 configuration. All operations are
     * sequential; no LCD writes occur during capture or sensor setup. */
    stdio_init_all();
    AppStatus camera_status = camera_init();
    if (lcd1602_init() != APP_OK) {
        puts("LCD initialization failed; camera stopped.");
        camera_deinit();
        return 1;
    }
    if (camera_status != APP_OK) { return stop_with_error("Camera error"); }
    if (ocr_init() != APP_OK) { return stop_with_error("OCR error"); }
    const OCRConfig settings = {.single_line = true};
    if (ocr_configure(&settings) != APP_OK) { return stop_with_error("OCR error"); }

    char displayed[33] = {0};
    bool display_valid = false;
    for (;;) {
        if (capture_and_display(displayed, &display_valid) != APP_OK) {
            puts("LCD write failed; camera stopped.");
            camera_deinit();
            return 1;
        }
        sleep_ms(500);
    }
#else
    return (int)state_machine_state(&machine);
#endif
}
