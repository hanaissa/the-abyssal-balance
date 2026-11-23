# Milestone 2 hardware demonstrations

Configure once from the repository root with the Pico SDK and toolchain:

```powershell
cmake -S . -B build/milestone2-pico -DABYSSAL_USE_PICO_SDK=ON -DPICO_BOARD=pico -DPICO_SDK_PATH="C:/path/to/pico-sdk"
cmake --build build/milestone2-pico --target RFID_Test WaterLevel_Test Potentiometer_Test Pump_Test LCDButtons_Test CameraOCR_Test UV_LED_Test
```

Each target produces `build/milestone2-pico/tests/<target>.uf2`.

| Target | Existing implementation | Demonstration |
| --- | --- | --- |
| RFID_Test | RFID_Test/main.c | RFID UID over USB |
| WaterLevel_Test | WaterLevel_Test/lcd_demo.c | Water level and RFID target on LCD, existing calibration |
| Potentiometer_Test | Potentiometer_Test/main.c | Knob readings and direction requests over USB |
| Pump_Test | Pump_Test/bench.c | Physical fill/drain bench commands over USB |
| LCDButtons_Test | LCDButtons_Test/main.c | LCD and physical buttons |
| CameraOCR_Test | CameraOCR_Test/main.c | Exact copy of src/main.c: Camera -> OCR -> LCD |
| UV_LED_Test | UV_LED_Test/main.c | Existing UV LED demonstration |

Hardware programs use production drivers and existing configuration. They are
not desktop CTest checks. Flash only the selected demonstration's UF2.
See component READMEs for operating instructions; CameraOCR keeps the production
application's behavior, including its StateMachine initialization.

Useful software regressions and dependencies remain at their original paths.
A host build retains Potentiometer_Test, Pump_Test, LCDButtons_Test,
WaterLevelDriver_Test, ButtonsDebounce_Test and OCR_Test. Run them with:

```powershell
cmake -S . -B build/milestone2-host -DABYSSAL_USE_PICO_SDK=OFF
cmake --build build/milestone2-host --config Release
ctest --test-dir build/milestone2-host -C Release --output-on-failure
```

The separate [camera regressions](CameraOCR_Test/picocamera/README.md) and
[OCR tools](../tools/ocr/CMakeLists.txt) retain their existing checks and fixtures.
The maintained [camera preview](../tools/camera/README.md) remains outside tests.
