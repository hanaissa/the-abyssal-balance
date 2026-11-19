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

## Motor hardware

The design uses two fixed-direction pumps. The current [firmware configuration](../include/AppConfig.h) reserves GP8 (header pin 11) for fill enable and GP9 (header pin 12) for drain enable. The [team pin map](PinMapping.md#two-fixed-direction-pumps-on-one-l298n) proposes channel A (`ENA`) for fill and channel B (`ENB`) for drain on one L298N, with each channel's direction inputs held at fixed levels. The exact module, supply and physical hookup still need confirmation.

Enabled PWM channels must be mutually exclusive. Keep both outputs off at startup and on stop. When switching between fill and drain, disable the active channel before enabling the other. Request stop on a rejected movement command or bridge failure. Review the [full pin allocation](PinMapping.md) and the firmware config before changing GPIO. Get the owner's approval for new or reassigned pins and for resolving any firmware/wiring conflict.

## Ownership

Pump work covers [`drivers/motor/`](../drivers/motor/) and [`tests/Pump_Test/`](../tests/Pump_Test/) on `feature/pumps`. The pump module validates and applies commands. The state-machine owner decides when to run the pump and owns the behavior of Submit, Reset and failure feedback. Use the [team ownership map](TeamDivision.md) for shared configuration and other modules.

## Motor verification

The checked-in L298N backend returns `APP_NOT_READY`. A host test can check the pump command contract with a fake bridge. A Pico build checks compilation. Before powering the pumps, verify the actual module connections and observe both enable signals with the motors disconnected, including startup, stop and direction changes. Verify actual fill, drain and stop behavior on the tank before calling the motor feature complete.
