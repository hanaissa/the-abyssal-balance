# Fixed-direction 12 V pump test

Read the [Master Project Context](../../docs/MasterContext.md) before motor work
or a motor handoff. Pin assignments agree with the
[pin map](../../docs/PinMapping.md) and [AppConfig.h](../../include/AppConfig.h).

## Hardware reported by the owner

- Two YX DC12V 385 (14150) pumps, powered by a separate nominal 12 V DC supply.
- Fill on L298N OUT1/OUT2; drain on OUT3/OUT4.
- ENA on Pico GP8 (physical 11), ENB on GP9 (physical 12); enable caps removed.
- IN1/IN3 fixed to Pico 3V3 OUT (physical 36); IN2/IN4 fixed to common GND.
- External supply positive to +12V/VS; supply negative, L298N GND, and Pico
  GND (physical 38) share ground. No pump power comes from Pico 3V3 OUT.
- Two 10 kohm enable-to-ground pull-downs are planned; installation is not
  confirmed. Fit them before relying on power-up or unplug behavior.

The L298N module's logic regulator/jumper configuration is **unverified**.
Confirm it for the actual board before applying motor power. The module's 5 V
terminal is not assumed connected to the Pico. Do not connect 12 V to any Pico
pin. Power off USB and external supplies before changing connections.

## Driver contract

`pump_init()` configures GP8/GP9 for hardware PWM, initially off.
`pump_start_fill(percent)` and `pump_start_drain(percent)` use the existing
`pump_apply(PumpCommand)` validation. `pump_stop()` stops either pump (both
enables low). A zero-percent start also stops both. Commands above 100% or an
invalid direction stop both and return an error; backend errors propagate.

Each pump has its own PWM duty command, but the project requires **only one
pump active at a time**. Starting the other pump stops the first, with a 10 us
both-off interval. IN1..IN4 are not controlled by software. Both enables use
PWM slice 4 at nominal 10 kHz; same-channel duty updates preserve PWM phase.
Use one control loop. The production driver has no automatic timeout: its
caller must stop it. The bench program below adds a 500 ms timeout and stops
when USB serial disconnects. It ignores movement commands during a pulse.

## Software verification

From the repository root, using an already configured host build:

```powershell
cmake --build build/host-check --target Pump_Test
ctest --test-dir build/host-check -R '^Pump_Test$' --output-on-failure
```

The host test substitutes a fake L298N. It checks the pump API, boundaries,
switch requests and errors, not physical PWM or water flow. See the master
context for fresh host configuration instructions.

## Build, flash and operate

The standalone CMake target compiles only the motor sources and Pico support.
Use the installed Pico SDK/toolchain and original Pico board configuration:

```powershell
cmake -S tests/Pump_Test -B build/pump-bench -DPICO_BOARD=pico
cmake --build build/pump-bench
```

With motor power off and motors disconnected, hold BOOTSEL while connecting
USB. Copy `build/pump-bench/Pump_Bench.uf2` to RPI-RP2. After restart, run this
in an interactive PowerShell terminal (change COM3 if needed):

```powershell
& .\tests\Pump_Test\Run-Bench.ps1 -Port COM3
```

| Key | Requested output for 500 ms |
| --- | --- |
| `f` | Fill GP8 at 50%; drain off |
| `d` | Drain GP9 at 50%; fill off |
| `F` | Fill GP8 at 100%; drain off |
| `D` | Drain GP9 at 100%; fill off |
| `s` | Stop both immediately |
| `q` | Terminal sends stop and closes |

No Enter is needed. Firmware prints the request before enabling, then reports
stop after timeout or `s`. Messages are software reports, not measurements.
Verify both-low startup, exclusive enables, timeout and early stop using the
[master bench procedure](../../docs/MasterContext.md#verify-enable-signals-with-motor-power-off).
After those checks and the logic-supply check pass, power everything off,
reconnect the pumps, prime according to their instructions, and run short wet
tests. Record actual flow direction, useful starting duty, and stop behavior.

## Limitations and evidence

L298N voltage drop and heating depend on motor current; 100% duty does not
guarantee 12 V at a pump. PWM duty is not a calibrated speed or flow reading.
Low duty may fail to start a pump. Startup/stall current, supply capacity,
module cooling and flyback protection need verification. Route pump current
through suitable wires and terminals, not breadboard contacts.
An enable-low stop removes drive; it does not guarantee immediate mechanical
stop or prevent siphoning. Software cannot detect a stalled pump or a failed
bridge with the present wiring; cutting external power is the hardware stop.

The [ST L298 datasheet, Tables 3-4 and section 5](https://www.st.com/resource/en/datasheet/l298.pdf)
specifies separate motor/logic supplies, compatible logic thresholds, voltage
drop, and protection requirements. It does not establish this pump's current
or this module's regulator/jumper arrangement.

Earlier serial output confirmed the bench firmware started and accepted a
terminal session. Physical enable waveforms, motor current, voltage at pumps,
water flow and stop behavior have **not** been verified in this session.
