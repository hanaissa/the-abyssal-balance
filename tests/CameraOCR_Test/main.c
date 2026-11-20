#include <stdio.h>
#include "AppConfig.h"

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

enum { CAMERA_ADDRESS = 0x21, BUS_TIMEOUT_US = 20000 };

static bool read_register(uint8_t reg, uint8_t *value) {
    /* Separate STOP/START phases for SCCB register selection and reading. */
    int result = i2c_write_timeout_us(i2c1, CAMERA_ADDRESS, &reg, 1, false,
                                      BUS_TIMEOUT_US);
    if (result != 1) {
        printf("FAIL: select register 0x%02X (%d).\n", reg, result);
        return false;
    }
    result = i2c_read_timeout_us(i2c1, CAMERA_ADDRESS, value, 1, false,
                                 BUS_TIMEOUT_US);
    if (result != 1) {
        printf("FAIL: read register 0x%02X (%d).\n", reg, result);
        return false;
    }
    return true;
}

static void observe_signals(void) {
    const unsigned pins[] = { PIN_CAMERA_VSYNC, PIN_CAMERA_HREF, PIN_CAMERA_PCLK };
    unsigned transitions[3] = {0};
    uint32_t previous = gpio_get_all();
    absolute_time_t deadline = make_timeout_time_ms(500);
    while (!time_reached(deadline)) {
        uint32_t sample = gpio_get_all();
        uint32_t changed = sample ^ previous;
        for (unsigned i = 0; i < 3; ++i) {
            if (changed & (1u << pins[i])) { ++transitions[i]; }
        }
        previous = sample;
    }
    printf("Observed transitions / 500 ms: VSYNC=%u HREF=%u PCLK=%u\n",
           transitions[0], transitions[1], transitions[2]);
    puts("Polling misses edges; these are not frequency measurements or an image test.");
}

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) { sleep_ms(100); }
    sleep_ms(1000);
    puts("OV7670 identification test. Keep external pump power OFF.");
    puts("No capture or OCR is implemented by this diagnostic.");

    /* Use only the camera pins reserved in AppConfig.h. */
    unsigned slice = pwm_gpio_to_slice_num(PIN_CAMERA_XCLK);
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t divider16 = (uint32_t)(((uint64_t)sys_hz * 8u + 6000000u) / 12000000u);
    if (divider16 < 16u || divider16 > 4095u) {
        puts("FAIL: system clock cannot generate the requested XCLK.");
        return 1;
    }
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 1);
    pwm_config_set_clkdiv_int_frac(&config, divider16 >> 4u, divider16 & 15u);
    pwm_init(slice, &config, false);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_CAMERA_XCLK), 1);
    gpio_set_function(PIN_CAMERA_XCLK, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
    printf("XCLK configured on GP%d: approximately %lu Hz (not measured).\n",
           PIN_CAMERA_XCLK, (unsigned long)((uint64_t)sys_hz * 8u / divider16));

    i2c_init(i2c1, 100000);
    gpio_set_function(PIN_CAMERA_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_CAMERA_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_CAMERA_SDA);
    gpio_pull_up(PIN_CAMERA_SCL);
    const unsigned inputs[] = { PIN_CAMERA_VSYNC, PIN_CAMERA_HREF, PIN_CAMERA_PCLK };
    for (unsigned i = 0; i < 3; ++i) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_disable_pulls(inputs[i]);
    }
    sleep_ms(100);
    for (;;) {
        uint8_t pid, version, manufacturer_high, manufacturer_low;
        bool ok = read_register(0x0A, &pid) && read_register(0x0B, &version) &&
                  read_register(0x1C, &manufacturer_high) &&
                  read_register(0x1D, &manufacturer_low);
        if (ok) {
            printf("ID: PID=%02X VER=%02X manufacturer=%02X%02X\n",
                   pid, version, manufacturer_high, manufacturer_low);
            puts(pid == 0x76 && manufacturer_high == 0x7F && manufacturer_low == 0xA2 ?
                 "PASS: OV7670-family identification registers match." :
                 "FAIL: unexpected identity; confirm module and wiring.");
        } else {
            puts("No complete ID read. Check module power, GND, SIOD/SIOC, XCLK,");
            puts("RESET released, PWDN low, pull-ups and SCCB compatibility.");
            puts("A failed bus read alone does not prove the camera is broken.");
        }
        observe_signals();
        sleep_ms(2000);
    }
}
#else
int main(void) {
    puts("Camera identification requires the standalone Pico build; not tested on host.");
    return 0;
}
#endif
