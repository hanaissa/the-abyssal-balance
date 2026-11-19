# The Abyssal Balance

Modular Raspberry Pi Pico / RP2040 C11 skeleton for CSEN701 Milestone 2.

This repository contains shared API contracts, an initial state-machine shell, module-owned driver stubs, independent test targets, and collaboration documentation. Existing hardware behavior is not claimed to work yet.

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

The L298N backend is a stub. Keep the pump supply off until the wiring and enable signals have been checked on the bench. See the [Master Project Context](docs/MasterContext.md#motor-verification) for the motor verification sequence.
