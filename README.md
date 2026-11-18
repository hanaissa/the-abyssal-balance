# The Abyssal Balance

Modular Raspberry Pi Pico / RP2040 C11 skeleton for CSEN701 Milestone 2.

This repository contains shared API contracts, an initial state-machine shell, module-owned driver stubs, independent test targets, and collaboration documentation. Existing hardware behavior is not claimed to work yet.

## Host build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Pico build

Set `PICO_SDK_PATH`, configure with `-DABYSSAL_USE_PICO_SDK=ON`, and use the Pico toolchain. Agree the exact board and SDK configuration before enabling hardware builds.

## Safety and scope

Pumps default to a stop request in the interface, but actuator behavior is still a stub. Do not connect powered pumps until electrical safety checks are complete. Camera GPIO mapping is intentionally omitted, and camera/OCR APIs return `APP_UNSUPPORTED` until a feasibility decision is made.
