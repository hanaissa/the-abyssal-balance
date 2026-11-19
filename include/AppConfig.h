#ifndef ABYSSAL_APP_CONFIG_H
#define ABYSSAL_APP_CONFIG_H
#define PIN_RFID_MISO 16
#define PIN_RFID_NSS 17
#define PIN_RFID_SCK 18
#define PIN_RFID_MOSI 19
#define PIN_RFID_RST 20
#define PIN_POTENTIOMETER 26
#define PIN_WATER_LEVEL 27
#define PIN_LCD_SDA 10
#define PIN_LCD_SCL 11
#define PIN_SUBMIT_BUTTON 14
#define PIN_RESET_BUTTON 15
/* Two fixed-direction pumps: direction inputs are hardwired, not GPIOs. */
#define PIN_PUMP_FILL_ENABLE 8
#define PIN_PUMP_DRAIN_ENABLE 9
#define PIN_UV_LED 22
/* OV7670 reservation only; camera capture/OCR are not implemented yet. */
#define PIN_CAMERA_D0 0
#define PIN_CAMERA_D1 1
#define PIN_CAMERA_D2 2
#define PIN_CAMERA_D3 3
#define PIN_CAMERA_D4 4
#define PIN_CAMERA_D5 5
#define PIN_CAMERA_D6 6
#define PIN_CAMERA_D7 7
#define PIN_CAMERA_SDA PIN_LCD_SDA
#define PIN_CAMERA_SCL PIN_LCD_SCL
#define PIN_CAMERA_VSYNC 12
#define PIN_CAMERA_HREF 13
#define PIN_CAMERA_PCLK 21
#define PIN_CAMERA_XCLK 28
#define WATER_LEVEL_TOLERANCE 10u
#define POTENTIOMETER_DEAD_ZONE_PERCENT 5u
#endif
