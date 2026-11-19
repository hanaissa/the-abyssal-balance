# The Abyssal Balance — Master Project Context

**Required reading before motor work.** This repository reference records the project decisions that govern the motor subsystem. The full workspace master document, `context.md` beside the checkout, contains the course brief index, source review and hardware evidence. Consult it when available and revisit this document whenever the motor plan changes.

## Project and motor behavior

The Abyssal Balance is CSEN701 Project 3. The finished station runs on a single Pico or Pico W in C/C++ and operates standalone. The player uses a knob to adjust the tank toward the target stored on an RFID card. The motor subsystem receives a direction and speed percentage through [`PumpCommand`](../include/AppTypes.h).

| Phase | Motor behavior |
| --- | --- |
| Startup and waiting for a card | Both pumps stopped. |
| Balancing | Clockwise fills, counter-clockwise drains, and knob position sets speed. Neutral stops both pumps. The application continues sampling the water level. |
| Submit | Stop while the application evaluates the reading associated with the press. |
| Failed Submit | Keep both pumps off during failure feedback. The LCD shows failure and the player retries the same target at the same water level. |
| Successful Submit | Keep both pumps off while the application runs the reward sequence. |
| Reset | The application requests a bounded drain and stops at the verified empty level. |

The project owner specified the same-level retry. Page 6 of the published project brief describes a drain penalty. The current motor plan follows the owner's retry decision.

## Pico GPIO assignments

These are the 26 exposed GPIO assignments in [AppConfig.h](../include/AppConfig.h) and the [team pin map](PinMapping.md), checked against `main` at `db1d5ed`. Header pins are physical positions on the Pico's 40-pin header. For grouped entries, the signals, GPIOs and positions are listed in the same order.

| Signal | Pico GPIO | Physical header pin |
| --- | --- | --- |
| Camera D0–D7 | GP0–GP7 | 1, 2, 4, 5, 6, 7, 9, 10 |
| **Fill pump enable / PWM (`ENA`)** | **GP8** | **11** |
| **Drain pump enable / PWM (`ENB`)** | **GP9** | **12** |
| Camera SIOD / LCD SDA | GP10 (I²C1) | 14 |
| Camera SIOC / LCD SCL | GP11 (I²C1) | 15 |
| Camera VSYNC | GP12 | 16 |
| Camera HREF / HS | GP13 | 17 |
| Submit button | GP14 | 19 |
| Reset button | GP15 | 20 |
| RC522 MISO / NSS / SCK / MOSI / RST | GP16 / GP17 / GP18 / GP19 / GP20 | 21 / 22 / 24 / 25 / 26 |
| Camera PCLK | GP21 | 27 |
| UV LED driver-control input | GP22 | 29 |
| Potentiometer wiper | GP26 (ADC0) | 31 |
| Water-level sensor signal | GP27 (ADC1) | 32 |
| Camera XCLK | GP28 | 34 |

GP8 and GP9 are the only Pico outputs allocated to the H-bridge. GP10/GP11 are shared camera/LCD I²C, and GP12/GP13 are camera sync. Review the entire table before planning a GPIO change. Get the owner's approval for new or reassigned pins and for resolving any firmware/wiring conflict.

## Motor and H-bridge wiring

The owner confirmed **two fixed-direction pumps**, one for fill and one for drain. The table below records the team's proposed physical hookup on one L298N module. The GPIO assignments are checked against the code. The actual module, pump ratings and assembled wiring still need bench verification. [Team wiring proposal](PinMapping.md#two-fixed-direction-pumps-on-one-l298n)

| L298N module terminal | Proposed physical connection |
| --- | --- |
| `ENA` (channel A) | Pico GP8, header pin **11**. Remove the `ENA` jumper and add a separate **10 kΩ pull-down** from `ENA` to common ground. |
| `ENB` (channel B) | Pico GP9, header pin **12**. Remove the `ENB` jumper and add a separate **10 kΩ pull-down** from `ENB` to common ground. |
| `IN1` and `IN3` | Fixed logic HIGH from Pico `3V3 OUT`, header pin **36**. |
| `IN2` and `IN4` | Fixed LOW to common ground. |
| `OUT1` and `OUT2` | The two fill-pump leads. Confirm actual flow direction on the bench. |
| `OUT3` and `OUT4` | The two drain-pump leads. Confirm actual flow direction on the bench. |
| `GND` | Common ground between the L298N, the pump supply negative and a Pico GND pin, such as header pin **38**. |
| Motor supply | Separate supply rated for the pump voltage and startup current, within the module's ratings. |
| Logic supply | Follow the exact module's regulator and supply-jumper instructions. Pico 3.3 V is used for the input signals in this proposal. |

Disconnect USB and external power before changing connections. Verify the module's flyback protection before powering the pumps. The pull-downs keep both enables low while the Pico GPIOs are unconfigured. [ST L298 datasheet, Tables 3–5 and §5](https://www.st.com/resource/en/datasheet/l298.pdf)

Enabled PWM channels must be mutually exclusive. Keep both outputs off at startup and on stop. When switching between fill and drain, disable the active channel before enabling the other. Request stop on a rejected movement command or bridge failure.

## Ownership

Pump work covers [`drivers/motor/`](../drivers/motor/) and [`tests/Pump_Test/`](../tests/Pump_Test/) on `feature/pumps`. The pump module validates and applies commands. The state-machine owner decides when to run the pump and owns the behavior of Submit, Reset and failure feedback. Use the [team ownership map](TeamDivision.md) for shared configuration and other modules.

## Motor verification

The checked-in L298N backend returns `APP_NOT_READY`. A host test can check the pump command contract with a fake bridge. A Pico build checks compilation. Before powering the pumps, verify the actual module connections and observe both enable signals with the motors disconnected, including startup, stop and direction changes. Verify actual fill, drain and stop behavior on the tank before calling the motor feature complete.
