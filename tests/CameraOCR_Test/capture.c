#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "AppConfig.h"
#include "../../drivers/uv/UV_LED.h"
#include "camera_capture.pio.h"
#include "frame_geometry.h"
#include "sync_filter.h"

enum { FRAME_BYTES = 640 * 480 / 2, ADDRESS = 0x21, BUS_TIMEOUT_US = 20000 };
static uint8_t frame[FRAME_BYTES];
static PIO capture_pio = pio0;
static int capture_sm = -1, capture_dma = -1;
static unsigned capture_offset;
static const struct pio_program *loaded_program;
static const FrameMode qvga = {320, 240, 8, 320 * 240};
static const FrameMode vga = {640, 480, 4, FRAME_BYTES};
static const FrameMode byte_check = {320, 240, 16, FRAME_BYTES};

/* The PIO WAIT instructions use these fixed camera pins. Fail the build if
 * the shared pin map changes rather than silently listening on wrong pins. */
_Static_assert(PIN_CAMERA_D0 == 0 && PIN_CAMERA_D1 == 1 &&
               PIN_CAMERA_D2 == 2 && PIN_CAMERA_D3 == 3 &&
               PIN_CAMERA_D4 == 4 && PIN_CAMERA_D5 == 5 &&
               PIN_CAMERA_D6 == 6 && PIN_CAMERA_D7 == 7 &&
               PIN_CAMERA_VSYNC == 12 && PIN_CAMERA_HREF == 13 &&
               PIN_CAMERA_PCLK == 21, "Update capture PIO for changed camera map");

static bool reg_write(uint8_t reg, uint8_t value) {
    uint8_t bytes[] = {reg, value};
    if (i2c_write_timeout_us(i2c1, ADDRESS, bytes, 2, false, BUS_TIMEOUT_US) != 2) {
        printf("ERROR register write %02X\n", reg);
        return false;
    }
    sleep_ms(2);
    return true;
}

static bool reg_read(uint8_t reg, uint8_t *value) {
    return i2c_write_timeout_us(i2c1, ADDRESS, &reg, 1, false, BUS_TIMEOUT_US) == 1 &&
           i2c_read_timeout_us(i2c1, ADDRESS, value, 1, false, BUS_TIMEOUT_US) == 1;
}

static bool configure_camera(void) {
    unsigned slice = pwm_gpio_to_slice_num(PIN_CAMERA_XCLK);
    uint32_t divider16 = (uint32_t)(((uint64_t)clock_get_hz(clk_sys) * 8 + 6000000) / 12000000);
    if (divider16 < 16 || divider16 > 4095) { return false; }
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 1);
    pwm_config_set_clkdiv_int_frac(&cfg, divider16 >> 4, divider16 & 15);
    pwm_init(slice, &cfg, false);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_CAMERA_XCLK), 1);
    gpio_set_function(PIN_CAMERA_XCLK, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
    i2c_init(i2c1, 100000);
    gpio_set_function(PIN_CAMERA_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_CAMERA_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_CAMERA_SDA);
    gpio_pull_up(PIN_CAMERA_SCL);
    sleep_ms(100);
    uint8_t pid, midh, midl;
    if (!reg_read(0x0A, &pid) || !reg_read(0x1C, &midh) || !reg_read(0x1D, &midl) ||
        pid != 0x76 || midh != 0x7F || midl != 0xA2) {
        puts("ERROR camera identity; run the identification diagnostic.");
        return false;
    }
    if (!reg_write(0x12, 0x80)) { return false; } /* COM7 software reset. */
    sleep_ms(200);
    /* Minimal YUV422 configuration. Explicit VGA source window.
     * TSLB[3]=0 selects Y first; COM13[0] controls U/V order on revision 1.4.
     * This deliberately avoids an undocumented vendor tuning table. */
    const uint8_t settings[][2] = {
        {0x11, 0x01}, /* CLKRC: prescale input by two. */
        {0x12, 0x00}, /* COM7: VGA source, YUV output. */
        {0x17, 0x11}, {0x18, 0x61}, {0x32, 0x80}, /* 640-column source. */
        {0x19, 0x03}, {0x1A, 0x7B}, {0x03, 0x03}, /* 480-row source. */
        {0x0C, 0x04}, /* COM3: downsample enable. */
        {0x3E, 0x19}, /* COM14: manual scaling, scaled PCLK /2. */
        {0x70, 0x3A}, {0x71, 0x35}, /* No scaler test pattern. */
        {0x72, 0x11}, /* DCW: /2 horizontally and vertically. */
        {0xA2, 0x02}, /* PCLK delay. */
        {0x15, 0x20}, /* COM10: positive sync/HREF, suppress blanking PCLK. */
        {0x3A, 0x04}, /* TSLB: Y at even byte positions. */
        {0x40, 0xC0}, /* Full output range, YUV (not RGB). */
        {0x42, 0x00}  /* COM17: normal scene. */
    };
    for (unsigned i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        if (!reg_write(settings[i][0], settings[i][1])) { return false; }
    }
    uint8_t divider, com13;
    if (!reg_read(0x73, &divider) || !reg_read(0x3D, &com13) ||
        !reg_write(0x73, (divider & 0xF0) | 0x01) ||
        !reg_write(0x3D, com13 & (uint8_t)~0x01)) { return false; }
    uint8_t format, scale, order;
    if (!reg_read(0x12, &format) || !reg_read(0x3E, &scale) ||
        !reg_read(0x3A, &order) || format != 0 || scale != 0x19 || (order & 8)) {
        puts("ERROR camera configuration readback.");
        return false;
    }
    return true;
}

static bool configure_capture(void) {
    capture_sm = pio_claim_unused_sm(capture_pio, false);
    capture_dma = dma_claim_unused_channel(false);
    if (capture_sm < 0 || capture_dma < 0) { return false; }
    /* Load only the selected sampler; three programs would exceed PIO RAM. */
    for (unsigned pin = PIN_CAMERA_D0; pin <= PIN_CAMERA_D7; ++pin) {
        pio_gpio_init(capture_pio, pin);
        gpio_disable_pulls(pin);
    }
    const unsigned sync[] = { PIN_CAMERA_VSYNC, PIN_CAMERA_HREF, PIN_CAMERA_PCLK };
    for (unsigned i = 0; i < 3; ++i) {
        pio_gpio_init(capture_pio, sync[i]);
        gpio_disable_pulls(sync[i]);
    }
    return true;
}

static bool select_mode(const FrameMode *mode) {
    bool full = mode->bits == 4;
    uint8_t divider;
    return reg_read(0x73, &divider) &&
           reg_write(0x0C, full ? 0x00 : 0x04) &&
           reg_write(0x3E, full ? 0x00 : 0x19) &&
           reg_write(0x72, full ? 0x00 : 0x11) &&
           reg_write(0x73, (divider & 0xF0) | (full ? 0x00 : 0x01));
}

/* Arm during a fresh VSYNC HIGH that has stayed HIGH for at least 50 us.
 * First require a stable LOW, so reopening serial cannot arm partway through
 * an old pulse. PIO is then ready before that qualified HIGH falls. */
static bool wait_clean_sync(absolute_time_t deadline) {
    SyncFilter low = {0}, high = {0};
    bool armed = false;
    while (!time_reached(deadline) && stdio_usb_connected()) {
        uint32_t now = time_us_32();
        bool vsync = gpio_get(PIN_CAMERA_VSYNC);
        if (!armed) {
            armed = sync_level_stable(&low, !vsync, now, 50);
        } else if (sync_level_stable(&high, vsync, now, 50)) {
            return true;
        }
        tight_loop_contents();
    }
    return false;
}

static bool select_exposure(bool short_exposure) {
    uint8_t com8, com1, high, vref;
    if (!reg_read(0x13, &com8)) { return false; }
    if (!short_exposure) { return reg_write(0x13, com8 | 0x05); }
    if (!reg_read(0x04, &com1) || !reg_read(0x07, &high) ||
        !reg_read(0x03, &vref)) { return false; }
    /* Bright-light focus test: disable automatic gain/exposure, set unity
     * gain and AEC=16 sensor line periods. This is not autofocus. Exposure
     * in seconds depends on sensor timing. Preserve the window geometry. */
    if (!reg_write(0x13, com8 & (uint8_t)~0x05) ||
        !reg_write(0x00, 0x00) || !reg_write(0x03, vref & 0x3F) ||
        !reg_write(0x07, high & 0xC0) || !reg_write(0x10, 0x04) ||
        !reg_write(0x04, com1 & 0xFC)) { return false; }
    uint8_t check, exposure;
    if (!reg_read(0x13, &check) || !reg_read(0x10, &exposure) ||
        (check & 0x05) || exposure != 0x04) {
        puts("ERROR manual exposure readback.");
        return false;
    }
    return true;
}

static bool capture_frame(const FrameMode *mode, FrameMode *actual, bool last_attempt) {
    const char *failure = last_attempt ? "ERROR" : "RETRY";
    absolute_time_t deadline = make_timeout_time_ms(5000);
    unsigned offset;
    const struct pio_program *program = mode->bits == 16 ? &camera_capture_raw_program :
        mode->bits == 4 ? &camera_capture_vga_program : &camera_capture_program;
    if (loaded_program != program) {
        pio_sm_set_enabled(capture_pio, capture_sm, false);
        if (loaded_program) { pio_remove_program(capture_pio, loaded_program, capture_offset); }
        loaded_program = NULL;
        if (!pio_can_add_program(capture_pio, program)) { puts("ERROR PIO space."); return false; }
        capture_offset = pio_add_program(capture_pio, program);
        loaded_program = program;
    }
    offset = capture_offset;
    pio_sm_config cfg = mode->bits == 16 ? camera_capture_raw_program_get_default_config(offset) :
        mode->bits == 4 ? camera_capture_vga_program_get_default_config(offset) :
                         camera_capture_program_get_default_config(offset);
    sm_config_set_in_pins(&cfg, mode->bits == 4 ? PIN_CAMERA_D4 : PIN_CAMERA_D0);
    sm_config_set_jmp_pin(&cfg, PIN_CAMERA_HREF);
    /* Left shift puts a Y byte, or two high-nibble Y pixels, in FIFO low byte. */
    sm_config_set_in_shift(&cfg, false, true, 8);
    pio_sm_init(capture_pio, capture_sm, offset, &cfg);
    pio_interrupt_clear(capture_pio, 0);
    pio_sm_set_consecutive_pindirs(capture_pio, capture_sm, PIN_CAMERA_D0, 8, false);
    dma_channel_config dma_cfg = dma_channel_get_default_config(capture_dma);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(capture_pio, capture_sm, false));
    if (!wait_clean_sync(deadline)) {
        printf("%s no stable VSYNC pulse; check camera VSYNC wire and ground.\n", failure);
        return false;
    }
    dma_channel_configure(capture_dma, &dma_cfg, frame,
                          &capture_pio->rxf[capture_sm], mode->bytes, true);
    pio_sm_set_enabled(capture_pio, capture_sm, true);
    bool started = false, previous_href = false;
    SyncFilter end_sync = {0};
    unsigned short_syncs = 0;
    unsigned lines = 0;
    /* Stop at the FIRST frame boundary, even when DMA has not filled. The old
     * fixed-size-only loop could silently stack rows from multiple frames. */
    for (;;) {
        if (time_reached(deadline) || !stdio_usb_connected()) {
            pio_sm_set_enabled(capture_pio, capture_sm, false);
            dma_channel_abort(capture_dma);
            printf("%s frame timeout/disconnect; check D0-D7, sync pins and format.\n", failure);
            return false;
        }
        if (!started && pio_interrupt_get(capture_pio, 0)) { started = true; }
        if (started) {
            uint32_t pins = gpio_get_all();
            bool vsync = (pins & (1u << PIN_CAMERA_VSYNC)) != 0;
            if (!vsync && end_sync.active) { ++short_syncs; }
            if (sync_level_stable(&end_sync, vsync, time_us_32(), 50)) { break; }
            bool href = (pins & (1u << PIN_CAMERA_HREF)) != 0;
            if (href && !previous_href) { ++lines; }
            previous_href = href;
        }
        tight_loop_contents();
    }
    pio_sm_set_enabled(capture_pio, capture_sm, false);
    sleep_us(10); /* Allow the final FIFO byte to drain before checking count. */
    unsigned received = mode->bytes - dma_hw->ch[capture_dma].transfer_count;
    bool overflow = !pio_sm_is_rx_fifo_empty(capture_pio, capture_sm);
    if (dma_channel_is_busy(capture_dma)) { dma_channel_abort(capture_dma); }
    if (!camera_frame_geometry_valid(mode, lines, received, overflow)) {
        printf("%s frame geometry: lines=%u expected=%u bytes=%u expected=%u overflow=%u short_syncs=%u\n",
               failure, lines, mode->height, received, mode->bytes, (unsigned)overflow, short_syncs);
        return false;
    }
    *actual = *mode;
    actual->height = lines;
    actual->bytes = received;
    if (lines != mode->height) {
        printf("WARNING short capture window: saving %ux%u, not %ux%u; no padding.\n",
               mode->width, lines, mode->width, mode->height);
    }
    printf("FRAME_CHECK lines=%u bytes=%u bits=%u short_syncs=%u; qualified VSYNC interval.\n",
           lines, received, mode->bits, short_syncs);
    return true;
}

int main(void) {
    stdio_init_all();
    uv_led_init(); /* GP22 low; UV only on for an explicit 'u' command. */
    while (!stdio_usb_connected()) { sleep_ms(100); }
    sleep_ms(500);
    puts("Camera capture: external pump power OFF. No OCR.");
    if (!configure_camera() || !configure_capture()) {
        puts("ERROR initialization failed. Reconnect USB to retry.");
        for (;;) { sleep_ms(1000); }
    }
    sleep_ms(1500); /* Let auto exposure settle; optical quality needs bench QA. */
    puts("READY c/t/u=scene/bars/UV, s/v=short scene/UV; uppercase=VGA. Use Save-Frame.ps1.");
    for (;;) {
        int key = getchar_timeout_us(10000);
        int operation = key >= 'A' && key <= 'Z' ? key + ('a' - 'A') : key;
        if (operation != 'c' && operation != 't' && operation != 'u' &&
            operation != 's' && operation != 'v' && operation != 'j') { continue; }
        const FrameMode *mode = key >= 'A' && key <= 'Z' ? &vga : &qvga;
        if (operation == 'j') { mode = &byte_check; }
        bool short_exposure = operation == 's' || operation == 'v';
        if (!select_mode(mode) || !select_exposure(short_exposure) ||
            !reg_write(0x42, operation == 't' ? 0x08 : 0x00)) { continue; }
        if (uv_led_set(operation == 'u' || operation == 'v') != APP_OK) { puts("ERROR UV control."); continue; }
        puts(short_exposure ? "EXPOSURE fixed=16 sensor lines, unity gain; use bright visible lighting." :
                              "EXPOSURE automatic gain/exposure.");
        sleep_ms(1000);
        FrameMode actual;
        bool captured = false;
        for (unsigned attempt = 0; attempt < 3 && !captured; ++attempt) {
            captured = capture_frame(mode, &actual, attempt == 2);
        }
        uv_led_set(false);
        if (!captured) { continue; }
        if (operation == 'j') {
            /* In-place row deinterleave: left=even bytes, right=odd bytes.
             * This uses exactly the same exposure/frame, not two captures. */
            uint8_t row[640];
            for (unsigned y = 0; y < actual.height; ++y) {
                for (unsigned x = 0; x < 320; ++x) {
                    row[x] = frame[y * 640 + x * 2];
                    row[x + 320] = frame[y * 640 + x * 2 + 1];
                }
                for (unsigned x = 0; x < 640; ++x) { frame[y * 640 + x] = row[x]; }
            }
            actual.width = 640;
            actual.bits = 8;
            puts("BYTE_CHECK left=first byte, right=second byte; one is brightness, one is color data.");
        }
        uint32_t checksum = 0;
        for (unsigned i = 0; i < actual.bytes; ++i) { checksum += frame[i]; }
        printf("ABYSSAL_FRAME %u %u %u %lu %u\n", actual.width, actual.height,
               actual.bytes, (unsigned long)checksum, actual.bits);
        /* Bulk binary USB output: never use printf for pixel data. */
        for (unsigned offset = 0; offset < actual.bytes; offset += 256) {
            if (!stdio_usb_connected()) { break; }
            unsigned count = actual.bytes - offset;
            if (count > 256) { count = 256; }
            stdio_put_string((const char *)frame + offset, (int)count, false, false);
        }
        static const char trailer[] = "\nEND_FRAME\n";
        stdio_put_string(trailer, sizeof(trailer) - 1, false, false);
    }
}
