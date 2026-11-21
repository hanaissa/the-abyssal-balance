# PicoCamera OV7670 RGB565 test

This supersedes the earlier custom-capture experiments for new camera testing.
It uses the actual [PicoCamera Pico SDK example](https://github.com/umeiko/PicoCamera/tree/281d9ff6de5d5f6bd68c2ce75f245cc9cbadb0ea/examples/pico_sdk_capture)
and a pinned MIT-licensed library snapshot in `../vendor/PicoCamera`, with the
local sized-capture patch recorded in its [UPSTREAM.md](../vendor/PicoCamera/UPSTREAM.md).
The Pico SDK target links `pico_camera`; Arduino is not involved. Legacy bench
files are preserved, but do not flash them for these instructions.

**Current fix v3:** after both scene and scaler bars reported `starts=2 ends=1`,
capture setup now keeps PIO disabled until its counter and DMA are ready.
The sized sampler rechecks HREF after PCLK, retains the first line when HREF
asserts with VSYNC, and halts at the requested byte count. A PIO start flag
lets the CPU reject a short first frame before collecting another frame.
The wrapper programs and verifies a 12..492 (480-row) source window, CLKRC
divider four, reversed PCLK, and DSP bars rather than the scaler pattern.
The v2 hardware run still stopped short at 38280/38400 bytes within one frame.
This is consistent with a one-byte-per-row deficit, but did not measure individual
rows. Further review found that upstream `frame_control()` writes only HREF's
six window bits, clearing the two timing-offset bits. V3 restores HREF[7:6]
to the sensor's documented default `0x80` while preserving the window LSBs.
With this window, the full HREF register should read `0xB6`.
These changes fix concrete setup issues; the HREF offset's effect on the hardware
byte deficit remains to be verified. GPIO assignments
are unchanged. The receiver, CRC, raw-frame sidecars, metadata and original
single-frame checks are retained.
Run without `--color-bars` to capture the actual scene. That flag deliberately
produces a test pattern, not a photograph. Old images remain unchanged.

Rebuild and **reflash** before retrying; the startup banner must contain
`Capture fix v3`. `CAPTURE_CONFIG` must show `source_rows=480` and `HREF=0xB6`, and
`CAPTURE_DIAG` reports the received byte count, driver status, and VSYNC counts.
`HREF_rows` counts observed falling HREF edges while the first VSYNC interval is
active. It is diagnostic only: IRQ delivery order at a simultaneous frame/line
boundary can affect it. Status -3 rejects a short first frame; -2 is a timeout. No incomplete image
is padded or accepted. New hardware success is still required.

## Electrical checks before starting

Keep external pump power off. Disconnect USB and external supplies before
changing wiring. [PinMapping.md](../../../docs/PinMapping.md) and
[AppConfig.h](../../../include/AppConfig.h) are the assignment sources.

| Camera | Pico GPIO |
| --- | --- |
| D0..D7 | GP0..GP7, in order |
| SIOD/SDA | GP10, I2C1 |
| SIOC/SCL | GP11, I2C1 |
| VSYNC | GP12 |
| HREF | GP13 |
| PCLK | GP21 |
| XCLK | GP28, library-generated 10 MHz requested |

- Verify the actual module's supply and I/O ratings. The photo identifies a
  breakout marked OV7670/3.3V; its internal regulation and measured voltages
  are not established. The bare sensor's core, analog and I/O rails differ;
  a breakout supply label is not a bare-sensor power specification. No 5 V
  power/logic is assumed safe for the camera. Refer to the module schematic
  and [sensor electrical specification](https://web.mit.edu/6.111/www/f2016/tools/OV7670_2006.pdf).
- RESET is active LOW: it must be released HIGH at a valid module logic level.
  PWDN must be LOW for normal operation. Neither has an allocated Pico GPIO.
  The config uses `pin_reset = pin_pwdn = -1`: **software cannot drive or
  measure these signals**. Inspect their actual wiring/module pulls before
  sending `c`; the library's software reset cannot release hardware reset.
  Do not allocate another GPIO without owner approval.
- GP10/GP11 are shared with the LCD. Verify pull-ups on the Pico side are at
  a compatible voltage, not 5 V. A 5 V LCD backpack with 5 V pull-ups requires
  the documented bidirectional level shifter. Firmware cannot measure this
  voltage or validate level-shifter wiring. A bus ACK does not prove safety.
- The test owns initialization of I2C1 at 100 kHz, then gives PicoCamera an
  already initialized bus (`pin_sccb_sda=-1`, `sccb_i2c_port=1`), as supported
  by the upstream API. A stuck-LOW check runs before camera init. An optional
  read-only probe of LCD address 0x27 reports ACK/no ACK; no LCD driver or
  port write occurs. No RFID/motor/sensor/button/LCD driver is modified or
  initialized by this executable. No UV illumination is enabled.

These are required physical checks, **not claims that the wiring was verified**.

## Exact Windows commands

Run from `C:\Users\hossa\Desktop\the-abyssal-balance`. The following paths match
the installed SDK, toolchain, Ninja and uv found on this laptop.

```powershell
cmake -S tests/CameraOCR_Test/picocamera -B build/picocamera-bench -G Ninja -DCMAKE_MAKE_PROGRAM=C:/Users/hossa/.pico-sdk/ninja/v1.13.2/ninja.exe -DPICO_BOARD=pico -DPICO_SDK_PATH=C:/Users/hossa/.pico-sdk/sdk/2.3.1 -DPICO_TOOLCHAIN_PATH=C:/Users/hossa/.pico-sdk/toolchain/15_2_Rel1 -Dpicotool_DIR=C:/Users/hossa/.pico-sdk/picotool/2.3.1/picotool
cmake --build build/picocamera-bench --target PicoCamera_Test --parallel 4
```

Alternatively the existing root Pico build exposes the same target, opt-in:

```powershell
cmake -S . -B build/pico-check -DABYSSAL_BUILD_PICOCAMERA_TEST=ON
cmake --build build/pico-check --target PicoCamera_Test
```

That uses the existing Pico SDK configuration in `build/pico-check`. A fresh
root build also needs `ABYSSAL_USE_PICO_SDK=ON` and SDK/toolchain settings.
The standalone UF2 below is `build/picocamera-bench/PicoCamera_Test.uf2`;
the root build's UF2 is `build/pico-check/tests/PicoCamera_Test.uf2`.

Close all COM readers. Hold BOOTSEL while connecting USB, then:

```powershell
$cameraDrive = [System.IO.DriveInfo]::GetDrives() |
    Where-Object { $_.IsReady -and $_.VolumeLabel -eq 'RPI-RP2' } |
    Select-Object -First 1
if ($cameraDrive) {
    Copy-Item '.\build\picocamera-bench\PicoCamera_Test.uf2' $cameraDrive.RootDirectory.FullName
} else {
    Write-Host 'RPI-RP2 not found. Reconnect while holding BOOTSEL.'
}
```

Create the isolated Python environment and install dependencies:

```powershell
& "$env:USERPROFILE\.local\bin\uv.exe" venv --python 3.12 .venv-camera
& "$env:USERPROFILE\.local\bin\uv.exe" pip install --python .venv-camera/Scripts/python.exe -r tests/CameraOCR_Test/picocamera/requirements.txt
```

The environment creation may download Python. No global Python packages are
required. The environment and dependencies may already be installed from setup.

After the Pico restarts and electrical checks are complete:

```powershell
& .\.venv-camera\Scripts\python.exe tests/CameraOCR_Test/picocamera/receive.py --port COM3 --output build/picocamera.png
```

The receiver waits for the wiring-check banner and sends `c` to initialize and
capture one frame. It saves a PNG and opens it in an OpenCV window. Press a key
in that window to close it. The saved image is the original 160x120 color data,
not an upscaled or sharpened result. For the next capture, reconnect Pico USB
normally (no BOOTSEL), then run again; this firmware performs one test per boot.
Use `--no-display` to save without a GUI. If COM3 changed:

```powershell
& .\.venv-camera\Scripts\python.exe -m serial.tools.list_ports
```

RGB565 byte interpretation defaults to big-endian again. The first image looked
more plausible after swapping bytes, but the next raw frame looked more
plausible big-endian. Thus the earlier little-endian default was premature;
capture byte alignment remains suspect. Verify with red/green/blue objects;
`--byte-order little` remains available for comparison. Software color-vector
tests do not establish the physical sensor stream's order. The receiver also
saves a `.rgb565` sidecar containing the CRC-verified raw payload, so future
decoding checks need not recapture the image. No optical focus fix is assumed.

## Protocol, error checks and limitations

Firmware prints library version/commit, detection/initialization status and
expected dimensions/38400-byte length. It uses the real upstream API:
`pico_camera_init`, `pico_camera_sensor_info_get`, `pico_camera_fb_get`,
`pico_camera_fb_return`, `pico_camera_deinit`, and `pico_camera_err_str`.
Frame memory is returned and camera resources released before idling.

Firmware checks format/dimensions/length/pointer and observes VSYNC edges during
capture; it rejects a reported buffer that spans multiple active-frame rises.
The locally patched fixed-size capture has a 2-second timeout and rejects a
short first frame. Its diagnostic accessor distinguishes that rejection from
timeout and reports the actual DMA byte count. Register-table writes in the upstream OV7670 code do not
all propagate errors; init success is not proof every register was programmed.
No vendor source was patched to conceal those limitations.

Wire format: ASCII `ABYSSAL_RGB565 1 160 120 38400 CRC32HEX` followed by LF
(CRLF accepted), exactly 38400 raw bytes, then raw `\nEND_RGB565\n`. CRC32 is
IEEE/reflected with the standard `123456789` vector. Binary output disables
newline translation. Text/error messages occur only outside the payload.
The Python receiver bounds line size, validates version/dimensions/length,
reads exact binary bytes, checks CRC/trailer and enforces a 30-second deadline.
It refuses a malformed image instead of saving partial data. A failure requires
USB reconnect/retry; no automatic retransmission or OCR is implemented.

## Verification

Both standalone and root-target firmware are build checks only. Host checks:

```powershell
cmake -S tests/CameraOCR_Test/picocamera -B build/picocamera-host -G Ninja -DCMAKE_MAKE_PROGRAM=C:/Users/hossa/.pico-sdk/ninja/v1.13.2/ninja.exe -DABYSSAL_PICOCAMERA_HOST_TESTS=ON
cmake --build build/picocamera-host
ctest --test-dir build/picocamera-host --output-on-failure
& .\.venv-camera\Scripts\python.exe -m unittest discover -s tests/CameraOCR_Test/picocamera -p test_receive.py -v
```

Cases include the real sized-capture driver under a host SDK fake (disabled
setup ordering, normal completion, a 38280-byte short frame, no-sync timeout,
FIFO tail, cleanup and retry), plus its assembled sampler under synthetic
waveforms (first PCLK already high, HREF coincident with VSYNC, blanking clocks,
and halt at the exact byte count). The fake/simulator does not establish
electrical timing or actual DMA/FIFO behavior. Register tests cover the
478-to-480-row correction, preservation of gain/sync bits, scene/bar selection,
restoration of HREF's offset without changing its six window bits, and
configuration readback rejection. Existing cases include a valid frame, missing/truncated/wrong-format metadata, CRC32
vectors, binary payloads containing every byte value, corrupt checksum,
truncation, incorrect dimensions/trailer, device errors and known RGB color
vectors in both byte orders. The firmware also exercises real-library rejection
of GP10/11 paired with the wrong I2C peripheral before its valid init.

**Actual Pico capture, electrical checks, correct colors and usable focus still
require this new hardware test. Previous custom-capture images are not evidence
that PicoCamera works on this wiring. OCR is deliberately untouched.**

Setup results on this laptop: standalone and root Pico builds passed, the host
frame-contract/CRC checks passed, all four Python test groups passed, and the
existing six host checks passed. Initial vendored source hashes matched the downloaded
commit; subsequent local capture changes are documented in UPSTREAM.md.
`.venv-camera` has Python 3.12.16, pyserial 3.5, NumPy 2.5.5 and
OpenCV 4.14.0.94 installed. A synthetic RGB565 image saved and reloaded through
OpenCV without pixel changes; its GUI backend reports Win32. This is software
evidence only, not an actual camera image or a tested hardware viewer session.

## Historical experiments (reverted; not current configuration)

The owner tested this library build: OV7670 PID 0x0076 was detected and LCD
0x27 acknowledged, but the wrapper rejected a buffer with two observed VSYNC
starts and one end. Detection is verified for that run; a valid image is not.

Inspection found the pinned sensor driver's QQVGA source window is 12..490,
478 source rows, while requested QQVGA height is 120 after /4 downsampling.
The wrapper now uses the library's public `sensor_t.set_reg/get_reg` callbacks
to select 12..492 (480 source rows), preserve VREF high bits and verify readback.
It prints before/after row counts. The library callbacks return register values,
not status codes, so the wrapper validates register readback rather than testing
set_reg for a zero return. That window adjustment is in the wrapper.

This is a software correction to a concrete geometry mismatch and a candidate
explanation for needing bytes from a second frame, not proof of the hardware
failure's cause. The wrapper also waits for stable blanking before enabling
its frame counter. Strict one-frame validation and the 38400-byte contract
remain; no check was relaxed to accept the failed image. New regression checks
cover 478 versus 480 source rows and ignoring VREF gain bits. Both firmware
builds and host/Python tests passed again. Reflash the new UF2 before retesting;
running `cmake --build` alone does not update the board.

The owner confirmed 480-row readback but the next hardware attempt still
reported starts=2/ends=1. Thus the window correction alone did not solve capture.
The wrapper now rewrites CLKRC through the public API after RGB565/window
configuration, using internal predivider four (XCLK remains 10 MHz requested).
The source documents the PLL and RGB565 requirement to rewrite CLKRC; the
slower stream adds sampling margin. Actual PCLK is still unmeasured.

`CAPTURE_DIAG` adds HREF row counts and a read-only snapshot of the active PIO
DMA transfer count at the first frame end. It does not modify library capture
state. This distinguishes an actually short first frame from an extra counted
edge. Crossing into another frame remains a hard error; so does more than four
pending bytes at the first end, allowing only the small FIFO/DMA pipeline tail.
These changes build and software tests pass, but require another hardware run.

The next owner run reported all 120 HREF rows but 120 bytes pending at the
first frame end: 38280/38400 bytes were transferred before the second VSYNC
start. This is consistent with a one-byte-per-row boundary/phase loss; it
does not establish that every row lost exactly one byte. The wrapper now
tests reversed PCLK using COM10 bit4 through the public set_reg API, preserves
the library's VSYNC polarity and verifies full-register readback. This changes
the sensor clock phase, not any GPIO assignment or upstream source. The
first-frame and exact-length checks remain strict; the polarity test is not
claimed to solve capture until the resulting hardware diagnostics verify it.
The following owner run produced a 160x120 PNG with the reversed-PCLK firmware.
The decoded image was inspected: byte swapping removed false-color contours,
but substantial spatial blur remained. Thus image transfer has progressed;
usable focus, correct color calibration and OCR have not been demonstrated.

The next raw frame contradicted the little-endian interpretation: big-endian
removed false-color contours. Neither interpretation yielded a sharp scene.
Review found the upstream sized state machine enabled during its PC/FIFO/DMA
reset/setup. A small local patch disables it before setup and starts it only
after the byte count and DMA are ready, then stops it on success/timeout.
The sampler and sensor driver are still PicoCamera's; there is no new capture
driver. The patch builds and protocol tests pass, but must be tested on hardware;
it is not a demonstrated explanation or cure for all remaining blur.

## Historical internal-pattern isolation test (superseded by fix v3)

The latest owner scene is still unusable. Instead of further focus/exposure
guesses, `receive.py --color-bars` now sends `t` at the startup prompt; the
firmware originally called the library's `sensor_t.set_colorbar` function. It prints
`MODE internal sensor color bars` to distinguish the result from a photo.
This bypasses the optical scene to test the downstream data path; a clean
pattern does not establish focus, full ISP tuning or arbitrary-scene quality.

```powershell
& .\.venv-camera\Scripts\python.exe tests/CameraOCR_Test/picocamera/receive.py --port COM3 --color-bars --output build/picocamera-bars.png
```

This requires the new UF2. CRC, exact length, dimensions and one-frame checks
are unchanged. Do not interpret an internally generated pattern as a captured
view of the physical subject. The test is implemented and builds, not yet
verified on the physical module.

### DSP-pattern revision after 121/120 sync counts

The owner observed 121 VSYNC rises and zero HREF rows in the first interval
with the scaler pattern enabled, although normal-scene transfers had completed.
That observation alone does not prove the pins were swapped. The test now
disables the scaler generator through `sensor_t.set_colorbar(sensor, 0)` and
selects the independent DSP bars with `set_reg(COM17, 0x08, 0x08)`. This is the
pattern mechanism used in the earlier successful grayscale bar transfer. Both
operations use the real library API; no new capture backend is introduced.

`PATTERN_CHECK` verifies that scaler pattern bits stay clear, the DSP bit has
the requested value, and COM10 did not change. `SYNC_CHECK` prints separate
500 ms edge counts for the scene baseline and DSP bars during the same boot.
All frame/CRC checks remain. This change builds and tests pass; its hardware
effect is not established until the next run. Do not change wiring on the
basis of the previous scaler-test result alone.

After connecting in BOOTSEL mode, a shorter flash command is available:

```powershell
& .\tests\CameraOCR_Test\picocamera\Flash.ps1
```
