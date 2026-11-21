#include "Camera.h"
#include <stddef.h>
#include <string.h>

#if defined(ABYSSAL_USE_PICO_SDK) && ABYSSAL_USE_PICO_SDK
#include "AppConfig.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "PicoCamera.h"
#include "driver/pio_capture.h"
#include "CameraSetup.h"
#include "FrameContract.h"

#define CAMERA_IRQ_MASK ((1u << PIN_CAMERA_VSYNC) | (1u << PIN_CAMERA_HREF))
static bool ready;
static camera_fb_t *captured;
static CameraDiagnostics last;
static volatile unsigned starts, ends, rows;

/* A scoped raw handler leaves the application's global GPIO callback intact. */
static void camera_irq(void) {
    uint32_t events = gpio_get_irq_event_mask(PIN_CAMERA_VSYNC);
    gpio_acknowledge_irq(PIN_CAMERA_VSYNC, events);
    if (events & GPIO_IRQ_EDGE_RISE) { ++starts; }
    if ((events & GPIO_IRQ_EDGE_FALL) && starts) { ++ends; }
    events = gpio_get_irq_event_mask(PIN_CAMERA_HREF);
    gpio_acknowledge_irq(PIN_CAMERA_HREF, events);
    if ((events & GPIO_IRQ_EDGE_FALL) && starts) { ++rows; }
}

static bool wait_blanking(void) {
    uint32_t beginning = time_us_32(), low_since = beginning;
    while ((uint32_t)(time_us_32() - beginning) < 2000000u) {
        uint32_t now = time_us_32();
        if (gpio_get(PIN_CAMERA_VSYNC)) { low_since = now; }
        else if ((uint32_t)(now - low_since) >= 50u) { return true; }
        tight_loop_contents();
    }
    return false;
}

void camera_release_frame(void) {
    if (captured) { pico_camera_fb_return(captured); captured = NULL; }
}

void camera_deinit(void) {
    camera_release_frame();
    if (ready) { pico_camera_deinit(); }
    ready = false;
    memset(&last, 0, sizeof last);
}

AppStatus camera_init(void) {
    if (ready) { return APP_OK; }
    /* Same peripheral/frequency as the LCD. No LCD commands or bus deinit. */
    i2c_init(i2c1, 100000);
    gpio_set_function(PIN_CAMERA_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_CAMERA_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_CAMERA_SDA);
    gpio_pull_up(PIN_CAMERA_SCL);
    sleep_ms(5);
    if (!gpio_get(PIN_CAMERA_SDA) || !gpio_get(PIN_CAMERA_SCL)) { return APP_ERROR; }
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
        .frame_size = FRAMESIZE_QVGA, .jpeg_quality = 10,
        .fb_count = 1, .fb_location = PICO_CAMERA_FB_IN_SRAM
    };
    pico_camera_err_t error = pico_camera_init(&config);
    if (error != PICO_CAMERA_OK) { pico_camera_deinit(); return APP_ERROR; }
    const camera_sensor_info_t *info = pico_camera_sensor_info_get();
    if (!info || strcmp(info->name, "OV7670") != 0 ||
        !capture_setup(pico_camera_sensor_get(), false)) {
        pico_camera_deinit(); return APP_ERROR;
    }
    sleep_ms(1000); /* Preserve the verified sensor-settling interval. */
    ready = true;
    memset(&last, 0, sizeof last);
    return APP_OK;
}

AppStatus camera_capture(void) {
    if (!ready) { return APP_NOT_READY; }
    camera_release_frame();
    memset(&last, 0, sizeof last);
    if (!wait_blanking()) { last.capture_status = -2; return APP_ERROR; }
    starts = ends = rows = 0;
    gpio_acknowledge_irq(PIN_CAMERA_VSYNC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    gpio_acknowledge_irq(PIN_CAMERA_HREF, GPIO_IRQ_EDGE_FALL);
    gpio_add_raw_irq_handler_masked(CAMERA_IRQ_MASK, camera_irq);
    gpio_set_irq_enabled(PIN_CAMERA_VSYNC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_CAMERA_HREF, GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    camera_fb_t *fb = pico_camera_fb_get();
    gpio_set_irq_enabled(PIN_CAMERA_HREF, GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_enabled(PIN_CAMERA_VSYNC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_remove_raw_irq_handler_masked(CAMERA_IRQ_MASK, camera_irq);
    pio_capture_result_t result = pio_capture_last_result();
    last = (CameraDiagnostics){ (uint32_t)result.bytes_received, starts, ends, rows, result.status };
    if (!frame_is_expected(fb) || starts != 1 || ends > 1) {
        if (fb) { pico_camera_fb_return(fb); }
        return APP_ERROR;
    }
    captured = fb;
    return APP_OK;
}

AppStatus camera_get_frame(CameraFrame *frame) {
    if (!frame) { return APP_ERROR; }
    memset(frame, 0, sizeof *frame);
    if (!captured) { return APP_NOT_READY; }
    *frame = (CameraFrame){captured->buf, (uint32_t)captured->len,
                          (uint16_t)captured->width, (uint16_t)captured->height};
    return APP_OK;
}

AppStatus camera_set_color_bars(bool enabled) {
    if (!ready) { return APP_NOT_READY; }
    camera_release_frame();
    if (!capture_setup(pico_camera_sensor_get(), enabled)) { return APP_ERROR; }
    sleep_ms(1000);
    return APP_OK;
}

AppStatus camera_get_diagnostics(CameraDiagnostics *diagnostics) {
    if (!diagnostics) { return APP_ERROR; }
    *diagnostics = last;
    return ready ? APP_OK : APP_NOT_READY;
}

#else
AppStatus camera_init(void) { return APP_UNSUPPORTED; }
AppStatus camera_capture(void) { return APP_UNSUPPORTED; }
AppStatus camera_set_color_bars(bool enabled) { (void)enabled; return APP_UNSUPPORTED; }
void camera_release_frame(void) {}
void camera_deinit(void) {}
AppStatus camera_get_frame(CameraFrame *frame) {
    if (!frame) { return APP_ERROR; }
    memset(frame, 0, sizeof *frame);
    return APP_NOT_READY;
}
AppStatus camera_get_diagnostics(CameraDiagnostics *diagnostics) {
    if (!diagnostics) { return APP_ERROR; }
    memset(diagnostics, 0, sizeof *diagnostics);
    return APP_NOT_READY;
}
#endif
