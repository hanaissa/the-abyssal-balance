# Pin mapping

| Component | Pico GPIO |
|---|---:|
| RC522 MISO/NSS/SCK/MOSI/RST | GP16 / GP17 / GP18 / GP19 / GP20 |
| Potentiometer / water level | GP26 / GP27 (ADC0 / ADC1) |
| LCD SDA / SCL | GP4 / GP5 (I2C0) |
| Submit / Reset | GP14 / GP15 |
| Fill pump ENA/IN1/IN2 | GP10 / GP11 / GP12 |
| Drain pump ENB/IN3/IN4 | GP13 / GP8 / GP9 |
| UV LED driver | GP22 |

Camera pins are intentionally unassigned until OV7670 wiring, bus mode, timing, and GPIO budget are verified. The PCF8574T LCD backpack voltage and I2C pull-ups must be checked before connecting to the Pico.
