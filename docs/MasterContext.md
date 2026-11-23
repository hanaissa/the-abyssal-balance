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

The owner confirmed **two fixed-direction pumps**, one for fill and one for drain, and reported connecting the wiring. The photos show an HW-095 board with an L298N. The table records the intended hookup, which matches the firmware pin assignments. The assembled connections, supply ratings and flow direction still need bench verification. [Team wiring reference](PinMapping.md#two-fixed-direction-pumps-on-one-l298n)

| L298N module terminal | Proposed physical connection |
| --- | --- |
| `ENA` (channel A) | Remove the `ENA` jumper. Connect the front signal post in line with `IN1`–`IN4` to Pico GP8, header pin **11**. Add a separate **10 kΩ pull-down** from the same signal post to common ground. |
| `ENB` (channel B) | Remove the `ENB` jumper. Connect the front signal post in line with `IN1`–`IN4` to Pico GP9, header pin **12**. Add a separate **10 kΩ pull-down** from the same signal post to common ground. |
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

Pump work covers [`drivers/motor/`](../drivers/motor/) on `feature/pumps`. The pump module validates and applies commands. The state-machine owner decides when to run the pump and owns the behavior of Submit, Reset and failure feedback. Use the [team ownership map](TeamDivision.md) for shared configuration and other modules.

## Motor verification

### Implemented behavior

Call [`pump_init()`](../drivers/motor/Pump.h) before issuing movement commands. `pump_apply()` forwards fill or drain at 1–100% duty. A stop command or a valid movement command at 0% stops both channels. An invalid direction, movement above 100%, or bridge error requests a stop. `pump_stop()` can be called before initialization. Call the API from one control loop.

The [Pico backend](../drivers/motor/L298N.c) uses hardware PWM slice 4 at 10 kHz nominal, with GP8 on channel A and GP9 on channel B. It forces both enable pins low at startup and on stop. Before starting or switching channels, it clears the previous duty and holds both enables low for 10 µs. Updating the duty of an already-running channel keeps its PWM phase intact. Duty is the fraction of time the enable is high. The tank test establishes which duty values can actually start the pumps and produce useful flow.

### Verify water flow on the tank

After the enable signal check passes, disconnect USB and all power before attaching pump leads. Confirm the L298N logic and pump supplies, grounds, flyback protection and pump ratings against the wiring table. Follow the pumps' priming and dry-run limits for the wet test. With the pump supply connected, try one short fill pulse and one short drain pulse. Check that the correct pump moves water in the intended direction, the other pump stays off, and stop cuts pump drive. Record the board, supply voltage, duty that starts each pump, and observed flow and stop behavior.

### Verification status

The six registered host checks passed, including `Pump_Test`. A separate GPIO/PWM mock exercised the 10 µs switch interval, buffered duty updates, and stop before a pending duty update took effect. The Pico firmware and both Pico and Pico W bench builds compiled. Physical enable signals, flow and stop behavior still require hardware verification. The station-level failed-Submit and Reset flows remain with the application owner.
