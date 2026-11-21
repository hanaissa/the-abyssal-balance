# The Abyssal Balance

Modular Raspberry Pi Pico / RP2040 C11 skeleton for CSEN701 Milestone 2.

Hardware implementations live under `drivers/`, with production dependencies
under `drivers/camera/vendor/` and `third_party/`. Test folders contain checks
and demonstrations, and are optional for the application build. The camera's
working 320x240 capture is now available through its production driver API.

## Start here

Read the [Master Project Context](docs/MasterContext.md) before planning or changing motor functionality. Follow the [agent instructions](AGENTS.md) when delegating work, and use the [pin map](docs/PinMapping.md) and [team ownership map](docs/TeamDivision.md) to check wiring and scope.

## Host build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Pico build

Set `PICO_SDK_PATH`, configure with `-DABYSSAL_USE_PICO_SDK=ON`, and use the Pico toolchain. Agree the exact board and SDK configuration before enabling hardware builds.

## Motor status

The L298N backend is available for bench verification. Keep the pump supply off until the wiring and enable signals have been checked. See the [Master Project Context](docs/MasterContext.md#motor-verification) for the standalone `Pump_Bench` build and commands.

## Building without tests

Use `-DABYSSAL_BUILD_TESTS=OFF` for a submission build, or remove `tests/`:
the root CMake also checks whether that directory exists before adding it.
Keep `drivers/`, `include/`, `src/`, `third_party/`, `CMakeLists.txt` and the
SDK import file. Keep `tools/camera/` if you want the optional camera preview.

```powershell
cmake -S . -B build/submission -DABYSSAL_USE_PICO_SDK=ON -DABYSSAL_BUILD_TESTS=OFF
cmake --build build/submission --target abyssal_balance
```

A fresh build still needs your usual Pico SDK/toolchain settings. See the
[camera driver API](drivers/camera/README.md) and
[camera preview](tools/camera/README.md).

| Module | Production implementation |
| --- | --- |
| Camera | `drivers/camera/`; native 320x240 capture, frame view/release and cleanup |
| RFID | `drivers/rfid/` plus `third_party/pico-mfrc522/`; identification and UID polling |
| Water level | `drivers/water/`; raw ADC reading and tolerance comparison |
| Potentiometer | `drivers/potentiometer/`; ADC percent and pump-command mapping |
| Pumps | `drivers/motor/`; command validation and L298N PWM/stop |
| LCD | `drivers/display/`; I2C initialization and text/level display |
| Buttons | `drivers/buttons/`; raw polling plus optional 20 ms debounce/new-press API |
| UV | `drivers/uv/`; GPIO initialization and on/off control |

The demo's button filtering has been preserved as `buttons_poll_debounced()`;
`buttons_poll()` retains raw level semantics. Deleting tests does not remove
these driver implementations. Hardware demonstration loops and test fixtures
are not required by the production build.

This is not yet a complete station application: `src/main.c` and the state
machine are still shells. Reliable handwriting OCR, reading the target level from RFID
card memory, water-height calibration and full station integration remain
unfinished. A successful build does not verify those missing behaviors.

An [experimental standalone block-handwriting OCR](drivers/camera/ocr/MODEL.md)
is now included in the camera driver. Its measured full-line accuracy and
acceptance are not sufficient to claim reliable handwriting recognition.
