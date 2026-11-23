# Water sensor and LCD demonstration

`WaterLevel_Test` builds the existing `lcd_demo.c` unchanged, including RFID and
calibration.

The production code is in [WaterLevel.c](../../drivers/water/WaterLevel.c) and
[WaterLevel.h](../../drivers/water/WaterLevel.h). The LCD entry point only calls
these APIs; it contains no sensor conversion or filtering implementation.

## Current behavior

Update: the owner-selected defaults now live in `drivers/water/WaterLevel.c`
and are applied by `water_level_init()`: dry ADC 1..90, 50% at 1850,
75% at 2000, full at 2100. Gaps interpolate and readings above 2100 clamp
to 1000. Both test firmwares use the production conversion; the serial test
prints raw ADC, scaled units and percentage. The LCD demo starts ready to
read a card; manual calibration with `0,5,7,1` remains optional. Reboot
restores these defaults. These settings are requested anchors, not new bench
measurements. The fresh-calibration procedure and RAM-session evidence below
describe the earlier behavior; custom calibration still lasts only until reboot.

The old fixed calibration and 2044-count full-scale threshold have been removed.
Actual trials produced overlapping 50% and 75% ranges, so those constants could
report full immersion at half depth. The production driver now requires a
validated calibration before returning a scaled level.

- `water_level_read_raw()` returns one unfiltered ADC sample (0..4095).
- `water_level_read()` filters five samples 10 ms apart and returns 0..1000
  only after calibration. Otherwise it returns `APP_NOT_READY` and clears the
  scaled output. Its raw median remains available for diagnostics.
- `water_level_get_last_raw()` returns that median; a sampling failure
  invalidates it.
- `water_level_capture_calibration_point()` captures 16 median-filtered readings
  over about 1.5 seconds, returning their minimum, center and maximum.
- `water_level_set_calibration()` accepts dry/50%/75%/full points only when all
  values are within 0..4095, centers lie within their bands and adjacent bands
  are strictly separated. Overlap, touching bands or malformed input disables
  calibration. Init and `water_level_clear_calibration()` also disable it.

Validated bands map to 0, 500, 750 and 1000; gaps interpolate between the band
edges. Values below/above the calibrated endpoints clamp to 0/1000. This is an
empirical mapping, not proof of depth accuracy or future repeatability. Values
within a recorded band cannot distinguish finer depths inside that band.
Custom calibration is RAM-only; reboot restores the owner-selected defaults. No card is rewritten.
The demo interprets the card's stored numeric value as a 0..1000 target, while
the legacy RFID record/API still labels its numeric payload as raw ADC units.

## Build and operate

From the root using the existing Pico build:

```powershell
cmake -S . -B build/pico-check -DABYSSAL_BUILD_TESTS=ON
cmake --build build/pico-check --target WaterLevel_Test CameraOCR_Test
```

| Mode | UF2 from repository root |
| --- | --- |
| RFID target + calibrated water LCD | `build/pico-check/tests/WaterLevel_Test.uf2` |
| Camera/OCR LCD | `build/pico-check/tests/CameraOCR_Test.uf2` |

With pump power OFF, hold BOOTSEL while reconnecting Pico USB, then copy the
chosen UF2 to RPI-RP2. Camera/OCR firmware remains separate; its LCD shows
`Code Captured:` followed by the actual recognized text on row 2 (16 characters
maximum). Failed reads clear row 2 and show `Read failed`.

For water calibration, open COM3 at 115200 baud with DTR enabled. The LCD first
shows a calibration prompt and raw ADC readings. Hold each depth fixed and send
one character (no Enter required), waiting for its `CAL` result before moving:

1. Completely dry strip: send `0` (also restarts calibration).
2. Exactly 50% immersion: send `5`.
3. Exactly 75% immersion: send `7`.
4. Full sensing-strip immersion, electronics dry: send `1`.

A successful result says `CAL ACCEPTED`. Then tap a provisioned target card;
the LCD shows `Target:` on row 1 and scaled `Current:` on row 2, updating about
twice per second. `OK` means the scaled value is within +/-3 of the target.
Sensor read failures show an error and retry. LCD failures stop operation while
keeping USB diagnostics available. This demo controls no pumps, UV or buttons.

A rejected result says `CAL REJECTED`; the LCD stays on `Cal failed` with raw
ADC diagnostics instead of a misleading scaled number. Check physical sensor
repeatability before restarting with `0`. No code can uniquely map the same
raw reading to two different water depths.

## Hardware and evidence

Use the [pin map](../../docs/PinMapping.md) and
[configuration](../../include/AppConfig.h): sensor signal GP27/ADC1, LCD SDA
GP10 and SCL GP11 on I2C1 at address 0x27; existing RC522 SPI wiring is unchanged.
Keep sensor ADC output within 0..3.3 V. A 5 V LCD with 5 V pull-ups requires
appropriate I2C level shifting to the 3.3 V Pico/camera side. Power off before
rewiring and keep all electronics/connectors out of water. Keep the external
12 V pump supply OFF throughout this test.

The 2026-11-21 guided trial recorded dry 45..62, half depth 2187..2261 and
75% depth 2242..2301 ADC counts. A separate 75% recheck recorded 2162..2248.
Both 75% ranges overlap the half-depth range; these are not valid calibration
profiles and are not applied automatically.

After the owner requested a clean retry with no overlap rule, a fresh guided
session returned these points and the production driver reported `CAL ACCEPTED`:

| Depth | ADC minimum | ADC center | ADC maximum | Scaled units |
| --- | --- | --- | --- | --- |
| Dry | 1 | 41 | 90 | 0 |
| 50% | 1954 | 1968 | 1992 | 500 |
| 75% | 2074 | 2082 | 2094 | 750 |
| Full | 2096 | 2112 | 2125 | 1000 |

These points are active in Pico RAM for this session, not compiled defaults or
flash storage. Rebooting clears them. The 75%/full gap is only two ADC counts;
repeatability beyond the short capture remains unverified. No automatic overlap
assignment, widened ranges or previous calibration values were used.

The registered `WaterLevelDriver_Test` compiles the production driver with
mock ADC hardware. It checks those overlapping ranges are rejected, calibration
is required, accepted bands stay distinct, all 4096 ADC inputs produce monotonic
bounded output under a valid profile, and NULL, sampling errors, raw diagnostics,
median capture and init/reset behavior. It does not verify physical depth.

```powershell
cmake --build build/host-check --target WaterLevelDriver_Test
ctest --test-dir build/host-check -R '^WaterLevelDriver_Test$' --output-on-failure
```
