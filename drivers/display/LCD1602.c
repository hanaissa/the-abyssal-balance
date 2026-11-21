
#include "LCD1602.h"
#include "AppConfig.h"
#if defined(ABYSSAL_USE_PICO_SDK) && ABYSSAL_USE_PICO_SDK
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define LCD_I2C_PORT i2c1
#define LCD_ADDR 0x27
#define LCD_SDA_PIN PIN_LCD_SDA
#define LCD_SCL_PIN PIN_LCD_SCL

#define RS 0x01
#define EN 0x04
#define BL 0x08

static bool ready = false;

// Send data through I2C
static AppStatus lcd_write(uint8_t data) {
    int n = i2c_write_timeout_us(
        LCD_I2C_PORT, LCD_ADDR, &data, 1, false, 10000
    );
    return n == 1 ? APP_OK : APP_ERROR;
}

// Send 4 bits to the LCD
static AppStatus lcd_nibble(uint8_t data) {
    data |= BL;

    if (lcd_write(data) != APP_OK) return APP_ERROR;
    if (lcd_write(data | EN) != APP_OK) return APP_ERROR;
    sleep_us(1);
    if (lcd_write(data) != APP_OK) return APP_ERROR;

    sleep_us(50);
    return APP_OK;
}

// Send a command or character
static AppStatus lcd_send(uint8_t data, bool character) {
    uint8_t mode = character ? RS : 0;

    if (lcd_nibble((data & 0xF0) | mode) != APP_OK)
        return APP_ERROR;

    return lcd_nibble(((data << 4) & 0xF0) | mode);
}

// Print one row, padded to 16 characters
static AppStatus lcd_line(uint8_t row, const char *text) {
    if (row > 1 || text == NULL) return APP_ERROR;

    if (lcd_send(0x80 | (row ? 0x40 : 0x00), false) != APP_OK)
        return APP_ERROR;

    bool end = false;

    for (int i = 0; i < 16; i++) {
        char c = ' ';

        if (!end) {
            if (*text) c = *text++;
            else end = true;
        }

        if (lcd_send((uint8_t)c, true) != APP_OK)
            return APP_ERROR;
    }
    return APP_OK;
}

// Initialize LCD
AppStatus lcd1602_init(void) {
    ready = false;

    i2c_init(LCD_I2C_PORT, 100000);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_SDA_PIN);
    gpio_pull_up(LCD_SCL_PIN);

    sleep_ms(50);

    for (int i = 0; i < 3; i++) {
        if (lcd_nibble(0x30) != APP_OK)
            return APP_ERROR;
        sleep_ms(5);
    }

    if (lcd_nibble(0x20) != APP_OK) return APP_ERROR;
    if (lcd_send(0x28, false) != APP_OK) return APP_ERROR;
    if (lcd_send(0x0C, false) != APP_OK) return APP_ERROR;
    if (lcd_send(0x06, false) != APP_OK) return APP_ERROR;
    if (lcd_send(0x01, false) != APP_OK) return APP_ERROR;

    sleep_ms(2);
    ready = true;
    return APP_OK;
}

// Show target and current water levels
AppStatus lcd1602_show_levels(uint16_t target, uint16_t current) {
    if (!ready) return APP_NOT_READY;

    char line[17];

    snprintf(line, sizeof(line), "Target: %u", (unsigned)target);
    if (lcd_line(0, line) != APP_OK) return APP_ERROR;

    snprintf(line, sizeof(line), "Current: %u", (unsigned)current);
    return lcd_line(1, line);
}

// Display text on first row
AppStatus lcd1602_show_text(const char *text) {
    if (!ready) return APP_NOT_READY;
    return lcd_line(0, text);
}

AppStatus lcd1602_show_lines(const char *first, const char *second) {
    if (!ready) return APP_NOT_READY;
    if (first == NULL || second == NULL) return APP_ERROR;
    if (lcd_line(0, first) != APP_OK) return APP_ERROR;
    return lcd_line(1, second);
}

#else

AppStatus lcd1602_init(void) {
    return APP_NOT_READY;
}

AppStatus lcd1602_show_levels(uint16_t target, uint16_t current) {
    (void)target;
    (void)current;
    return APP_NOT_READY;
}

AppStatus lcd1602_show_text(const char *text) {
    (void)text;
    return APP_NOT_READY;
}

AppStatus lcd1602_show_lines(const char *first, const char *second) {
    (void)first;
    (void)second;
    return APP_NOT_READY;
}

#endif
