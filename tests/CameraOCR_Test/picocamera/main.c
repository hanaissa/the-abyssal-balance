#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"


#include "PicoCamera.h"
#include "AppConfig.h"
#include "frame_contract.h"
#include "crc32.h"


static volatile unsigned starts, ends;
static void frame_edge(uint pin, uint32_t events) {
    if (pin != PIN_CAMERA_VSYNC) { return; }
    if (events & GPIO_IRQ_EDGE_RISE) { ++starts; }
    if ((events & GPIO_IRQ_EDGE_FALL) && starts) { ++ends; }
}
static void finish(void) {
    stdio_flush();
    for (;;) { sleep_ms(100); }
}

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) { sleep_ms(100); }
    sleep_ms(500);
    puts("PicoCamera OV7670 test: Pico SDK, external pump power OFF.");
    printf("Library %s, upstream commit 281d9ff6de5d5f6bd68c2ce75f245cc9cbadb0ea\n", PICO_CAMERA_VERSION_STRING);
    puts("No RESET/PWDN GPIOs allocated: check module RESET released HIGH and PWDN LOW.");
    puts("Check camera supply/logic rating and shared LCD bus: Pico side must not have 5V pull-ups.");
    puts("Those electrical checks cannot be measured by this firmware.");
    puts("Send c after checking wiring for scene, or t for internal color bars. One RGB565 frame.");
    int command;
    do { command = getchar_timeout_us(10000); } while (command != 'c' && command != 't');

    /* Own the shared bus here; library shared-bus mode preserves it on deinit.
     * Never initialize/write the LCD, buttons, pumps or other drivers. */
    i2c_init(i2c1, 100000);
    gpio_set_function(PIN_CAMERA_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_CAMERA_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_CAMERA_SDA);
    gpio_pull_up(PIN_CAMERA_SCL);
    sleep_ms(5);
    if (!gpio_get(PIN_CAMERA_SDA) || !gpio_get(PIN_CAMERA_SCL)) {
        puts("ERROR: I2C1 SDA/SCL held LOW. Check shorts, pull-ups, LCD level shifter and power.");
        finish();
    }
    camera_config_t config = {
        .pin_pwdn = -1, .pin_reset = -1,
        .pin_xclk = PIN_CAMERA_XCLK,
        .pin_sccb_sda = -1, .pin_sccb_scl = -1,
        .pin_d0 = PIN_CAMERA_D0, .pin_d1 = PIN_CAMERA_D1,
        .pin_d2 = PIN_CAMERA_D2, .pin_d3 = PIN_CAMERA_D3,
        .pin_d4 = PIN_CAMERA_D4, .pin_d5 = PIN_CAMERA_D5,
        .pin_d6 = PIN_CAMERA_D6, .pin_d7 = PIN_CAMERA_D7,
        .pin_vsync = PIN_CAMERA_VSYNC, .pin_href = PIN_CAMERA_HREF,
        .pin_pclk = PIN_CAMERA_PCLK, .xclk_freq_hz = 10000000,
        .sccb_i2c_port = 1, .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QQVGA, .jpeg_quality = 10,
        .fb_count = 1, .fb_location = PICO_CAMERA_FB_IN_SRAM
    };
    /* Real-library failure case: invalid peripheral for GP10/GP11 must reject
     * before starting XCLK or touching a sensor. */
    camera_config_t invalid = config;
    invalid.pin_sccb_sda = PIN_CAMERA_SDA;
    invalid.pin_sccb_scl = PIN_CAMERA_SCL;
    invalid.sccb_i2c_port = 0;
    if (pico_camera_init(&invalid) != PICO_CAMERA_ERR_INVALID_ARG) {
        puts("ERROR: library did not reject mismatched SCCB pins/port.");
        finish();
    }
    puts("Config rejection check passed. Initializing I2C1 camera...");
    pico_camera_err_t error = pico_camera_init(&config);
    if (error != PICO_CAMERA_OK) {
        printf("ERROR initialization: %s (%d).\n", pico_camera_err_str(error), error);
        puts("Check RESET/PWDN, camera supply, shared-bus levels/pull-ups and signal connections.");
        puts("No hardware detection success is claimed. Unplug USB to retry.");
        pico_camera_deinit();
        finish();
    }
    const camera_sensor_info_t *info = pico_camera_sensor_info_get();
    if (!info || strcmp(info->name, "OV7670") != 0) {
        puts("ERROR: expected OV7670; library detected a different/unknown sensor.");
        pico_camera_deinit(); finish();
    }
    printf("Detected: %s, PID=0x%04X. Configured RGB565 QQVGA, 160x120, 38400 bytes.\n",
           info->name, info->pid);
    sensor_t *sensor = pico_camera_sensor_get();
    if (!sensor || !sensor->set_colorbar || sensor->set_colorbar(sensor, command == 't') != 0) {
        puts("ERROR configuring library test-pattern mode.");
        pico_camera_deinit(); finish();
    }
    puts(command == 't' ? "MODE library internal bars; NOT a scene photo." : "MODE actual scene; library defaults.");
    uint8_t lcd_status;
    int lcd = i2c_read_timeout_us(i2c1, 0x27, &lcd_status, 1, false, 10000);
    printf("Shared LCD 0x27: %s (read-only probe; not an LCD functionality/voltage test).\n",
           lcd == 1 ? "ACK" : "no ACK");
    sleep_ms(1000);
    /* Library OV7670 setup makes VSYNC HIGH during active frame data.
     * Reject a fixed-count capture that spans more than one such interval. */
    starts = ends = 0;
    gpio_set_irq_enabled_with_callback(PIN_CAMERA_VSYNC,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, frame_edge);
    camera_fb_t *fb = pico_camera_fb_get();
    gpio_set_irq_enabled(PIN_CAMERA_VSYNC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    if (!fb) {
        puts("ERROR capture: no frame returned (library capture timeout/resource failure).");
        puts("Check PCLK, VSYNC, HREF, D0-D7; init detection alone does not verify image capture.");
    } else if (!frame_is_expected(fb) || starts != 1 || ends > 1) {
        printf("ERROR incomplete/unexpected frame: %zux%zu, %zu bytes, format=%d, starts=%u ends=%u.\n",
               fb->width, fb->height, fb->len, fb->format, starts, ends);
    } else {
        printf("CAPTURE RETURNED: %zux%zu, %zu bytes, RGB565; one observed VSYNC interval.\n",
               fb->width, fb->height, fb->len);
        puts("Visual quality, color order and handwriting OCR still require separate hardware checks.");
        printf("ABYSSAL_RGB565 1 %zu %zu %zu %08lX\n", fb->width, fb->height, fb->len,
               (unsigned long)image_crc32(fb->buf, fb->len));
        for (size_t offset = 0; offset < fb->len && stdio_usb_connected(); offset += 256) {
            size_t count = fb->len - offset;
            if (count > 256) { count = 256; }
            stdio_put_string((const char *)fb->buf + offset, (int)count, false, false);
        }
        static const char trailer[] = "\nEND_RGB565\n";
        stdio_put_string(trailer, sizeof(trailer) - 1, false, false);
    }
    if (fb) { pico_camera_fb_return(fb); }
    pico_camera_deinit();
    puts("Test finished. Unplug USB to repeat. No other subsystem was initialized.");
    finish();
}
