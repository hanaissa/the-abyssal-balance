# Pin mapping

GPIO assignments match `include/AppConfig.h`. Physical pins refer to the original
Pico's 40-pin header, with USB at the top. All 26 exposed GPIOs are allocated.

| Component | Pico GPIO | Physical pin |
|---|---|---|
| Camera D0-D7 | GP0-GP7 | 1, 2, 4, 5, 6, 7, 9, 10 |
| Fill pump ENA | GP8 | 11 |
| Drain pump ENB | GP9 | 12 |
| Camera SIOD / LCD SDA | GP10 (I2C1) | 14 |
| Camera SIOC / LCD SCL | GP11 (I2C1) | 15 |
| Camera VSYNC | GP12 | 16 |
| Camera HREF (HS) | GP13 | 17 |
| Submit button | GP14 | 19 |
| Reset button | GP15 | 20 |
| RC522 MISO / NSS / SCK / MOSI / RST | GP16 / GP17 / GP18 / GP19 / GP20 | 21 / 22 / 24 / 25 / 26 |
| Camera PCLK | GP21 | 27 |
| UV LED driver control | GP22 | 29 |
| Potentiometer | GP26 (ADC0) | 31 |
| Water level sensor | GP27 (ADC1) | 32 |
| Camera XCLK | GP28 | 34 |

## Two fixed-direction pumps on one L298N

One pump always fills; the other always drains. Each bridge channel needs only
one Pico enable/PWM signal because its direction inputs are held at fixed levels:

| L298N connection | Wiring |
|---|---|
| ENA | GP8, physical pin 11; remove the ENA jumper |
| ENB | GP9, physical pin 12; remove the ENB jumper |
| IN1 and IN3 | Fixed logic HIGH (Pico 3V3 OUT can provide a valid L298 logic HIGH) |
| IN2 and IN4 | Common GND |
| OUT1 / OUT2 | Fill pump |
| OUT3 / OUT4 | Drain pump |

Use a motor supply rated for the pumps and provide the L298N logic supply as
specified by the actual module. Do not power pumps from Pico GPIO or 3V3 OUT.
Connect the motor-driver and Pico grounds. A 10 kohm pull-down on each enable
keeps the corresponding pump disabled while the Pico pins are unconfigured.
If a pump runs in the wrong direction, disconnect power and reverse that pump's
output leads. Never enable both pumps together in the eventual driver.

The Pico motor driver uses the two enables for PWM and stop. Keep pump power
off during sensor-only tests and the initial [enable signal check](MasterContext.md#motor-verification).

## Shared camera/LCD bus and other connections

- Camera SIOD/SIOC and LCD SDA/SCL share I2C1. The LCD code currently uses 0x27;
  OV7670 conventionally uses 7-bit address 0x21. Camera SCCB support must still
  be implemented and tested with the shared bus.
- Keep the Pico/camera bus side at 3.3V. A 5V LCD backpack with 5V pull-ups needs
  a bidirectional I2C level shifter; place the LCD on its 5V side and the Pico
  and camera on its 3.3V side. Verify pull-ups and module supply requirements.
- Camera pin reservations do not implement XCLK generation, image capture or
  OCR. Camera RESET/PWDN levels also need to follow the actual module's wiring
  requirements; no additional GPIOs are reserved for them.
- Use USB serial: UART on GP0/GP1 would conflict with camera data lines.
- Each button connects its assigned GPIO to GND when pressed. The button driver
  enables internal pull-ups; no button VCC connection is needed.
- The UV LED GPIO is a driver-control signal, not a power output for a UV lamp.
- RUN is the hardware reset input, not an available GPIO. GND pins are not GPIOs.

References: [L298 data sheet](https://www.st.com/resource/en/datasheet/l298.pdf),
[OV7670 data sheet](https://www.olimex.com/Products/Components/Camera/CAMERA-OV7670/resources/OV7670.pdf).
