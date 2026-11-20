# OV7670 camera bench tests

For the current PicoCamera-based RGB565 160x120 capture and Python/OpenCV
viewer, use [the PicoCamera test instructions](picocamera/README.md).
The custom PIO experiments below are retained for history and are not the
recommended image-capture test.

The standalone identification diagnostic tests reserved camera wiring. The
separate experimental capture target described below transfers a single frame.
Neither target implements the production `camera_capture()` API or station reward flow.
`ocr_read_text()` cannot read the camera yet and returns
`APP_UNSUPPORTED`; it never returns a canned reward code.

Keep external pump power off throughout this test. Disconnect USB before
changing wiring. Verify the actual camera module's supply and logic ratings;
the bare OV7670 sensor and a regulator-equipped breakout have different power
requirements. Do not assume that any camera connector can accept 5 V.
Keep the Pico side of the shared LCD/camera bus at 3.3 V, with appropriate
level translation for a 5 V LCD backpack as described in the pin map.

Pin assignments come from [AppConfig.h](../../include/AppConfig.h) and the
[pin map](../../docs/PinMapping.md):

| Camera | Pico |
| --- | --- |
| SIOD / SDA | GP10 |
| SIOC / SCL | GP11 |
| VSYNC | GP12 |
| HREF | GP13 |
| PCLK | GP21 |
| XCLK | GP28 |
| D0..D7 | GP0..GP7 (unused by this diagnostic) |

The camera must be out of hardware reset and power-down. RESET/PWDN follow
the actual module's electrical requirements; this test allocates no new GPIOs
and does not drive those pins. Check the module's own pull-ups/pull-downs and
logic voltage before tying them to a supply. Proper SCCB pull-ups may be
needed; the weak Pico pull-ups enabled here are not proof of bus integrity.

## Build and run

Using an installed SDK/toolchain, configure separately from the main app:

```powershell
cmake -S tests/CameraOCR_Test -B build/camera-bench -DPICO_BOARD=pico
cmake --build build/camera-bench
```

Hold BOOTSEL while attaching USB and copy
`build/camera-bench/Camera_Diagnostic.uf2` to RPI-RP2. Open the new USB serial
port at 115200 with DTR enabled; it waits for that connection before testing.

The test generates approximately 12 MHz XCLK at 50% duty on GP28, then attempts
SCCB reads at 7-bit address 0x21 using I2C1 at 100 kHz. Each bus phase has a
20 ms timeout. It selects register addresses with STOP before reading and
does not write configuration-register values or scan other shared-bus devices.

Expected identification is PID 0x76, manufacturer 0x7FA2. VER is printed
without enforcing a particular revision. A match identifies the sensor family;
it does not establish valid image data. A failed read could be caused by
wiring, supply, reset/power-down state, clock, pull-ups, or SCCB compatibility;
it does not prove a defective sensor.

The test also reports observed VSYNC/HREF/PCLK transitions over 500 ms. Software
polling misses fast transitions; these counts are neither frequency measurements
nor an image-capture test. A zero count is inconclusive without checking the
sensor state and wiring. No camera configuration is applied, so timing depends
on its existing register state. Unplug USB to stop the diagnostic and XCLK.

Register addresses, IDs, and the 10..48 MHz XCLK range are from the
[OV7670 datasheet, Tables 4 and 5](https://www.olimex.com/Products/Components/Camera/CAMERA-OV7670/resources/OV7670.pdf).

## Verification

The standalone firmware cross-compiled for the original Pico. Hardware ID,
waveforms, frame capture and OCR on physical images have not been verified. The root host target
only reports that the diagnostic needs the Pico; its exit status is not camera
verification.

## Handwritten UV-revealed message

The owner clarified that the revealed text is handwritten, for example
MIX 9:5:2. A fixed-font block matcher does not meet this requirement and was
removed. Camera capture and handwriting recognition remain unimplemented.
The camera diagnostic tests identification and observed timing only.

Recognition needs actual handwritten samples under the intended UV lighting,
a readable camera frame, and a suitable recognizer evaluated against held-out
samples. No handwriting accuracy or live OCR functionality is claimed.
A host-assisted recognition prototype would change the standalone architecture
described in the integration plan and must not be represented as the finished station.

## Experimental single-frame capture

`Camera_Capture` is separate bench firmware; the ID-only target is retained.
It uses PIO0 (one claimed state machine, two 13-word programs) and one claimed
DMA channel to read luma on rising PCLK, gated by HREF and starting after a
VSYNC pulse. These resources are private to this standalone executable, not
allocated for the main app. No pin assignments are changed. The capture PIO
has build assertions for its fixed WAIT pin numbers.

The camera is reset and configured for YUV422 with an explicit VGA source
window. QVGA mode downsamples to 320x240; PIO skips chroma bytes and captures
76800 luma bytes. VGA mode disables downsampling and captures the high four
bits of each of 640x480 luma pixels, packed in 153600 bytes. This is native
640x480 spatial sampling with only 16 grayscale levels, not interpolation.
Full VGA 8-bit grayscale alone requires 307200 bytes and cannot fit in the
RP2040's 264 KiB SRAM along with the running program. Both current modes fit
the same 153600-byte frame buffer. Minimal configuration is based on the datasheet's
COM3/COM7/COM10/COM14, scaler, TSLB and COM13 registers; no full vendor ISP
tuning table is applied. Exposure, focus, cropping, actual byte order and line
geometry must be checked on hardware. A completed DMA transfer alone does
not validate a frame's contents or optical quality.

An earlier hardware scene showed vertically duplicated content. The original
fixed-byte row loop could miss the first clock, spill into the next line and
continue across frames to fill its buffer. It has been replaced with HREF-gated
luma sampling that has no initial wait-for-low. Firmware monitors the first
following VSYNC and counts HREF rows; it rejects unexpected rows, short payloads
or FIFO overflow with `ERROR frame geometry` rather than saving stacked frames.
`FRAME_CHECK` reports accepted geometry before image transfer. Row counting
uses CPU polling in this standalone firmware; a count error is rejected and
may need bench investigation. It does not prove every data bit was sampled correctly.

The revised VGA bench run reported 477 HREF rows and 152640 packed bytes:
exactly 477 complete 640-pixel rows, with no overflow. The diagnostic now
allows a short VSYNC capture window of up to eight rows when the byte count
matches all complete rows exactly. It sends and saves the actual dimensions
(for that run, 640x477), with a warning. It does not pad, stretch, silently
claim 480 rows or acquire rows from another frame. Larger deficits, partial
rows, row/byte disagreement, and overflow remain errors. Why those rows fall
outside the current capture window still requires timing/window investigation.

SCCB operations time out after 20 ms per phase. Capture times out after five
seconds and stops DMA/PIO on timeout or USB disconnect, permitting retry.
No foreground USB logging occurs during sampling. USB transfer follows only
after capture, with a length header and byte-sum checksum. The checksum detects
many transfer errors but is not a cryptographic or image-validity check.

Build with the existing standalone configuration:

```powershell
cmake --build build/camera-bench --target Camera_Capture
```

With the external pump supply off, flash
`build/camera-bench/Camera_Capture.uf2` using BOOTSEL. Close other COM3 readers,
then use these PowerShell commands from the repository root:

```powershell
# Internal DSP color bars appear as grayscale bands in the saved image.
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -ColorBars -Output build/camera-bars.bmp
# Actual scene under ordinary visible lighting.
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -Output build/camera-frame.bmp
# Higher spatial resolution, 16 grayscale levels; requires updated firmware.
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -Resolution VGA -Output build/camera-vga.bmp
```

Open the resulting BMP in the IDE or an image viewer. Capture a well-lit
high-contrast object first. If a frame is striped, repeated, shifted, or blank,
inspect its image before attempting OCR; geometry/configuration, D0..D7 wiring
and clock integrity may need adjustment. If internal bars work but the scene
does not, inspect illumination, exposure and focus as well.

Once ordinary capture works, `-UV` explicitly enables the existing GP22 UV
driver-control output for settling/capture, then requests it off before transfer.
Do not look into the UV source. The firmware cannot switch off an externally
powered UV driver after Pico power loss; use the physical supply switch.

```powershell
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -UV -Output build/camera-uv.bmp
```

The receiver uses .NET serial APIs and writes a 24-bit grayscale BMP without
third-party libraries. Its binary parser rejects unexpected dimensions,
incomplete payloads, bad trailers and checksum mismatch. Parser/BMP checks:

```powershell
& .\tests\CameraOCR_Test\Test-FrameTools.ps1
# Run after building: simulates actual assembled PIO instruction arrays.
python tests/CameraOCR_Test/test_capture_pio.py
# Optional standalone host geometry regression checks, in a separate build.
cmake -S tests/CameraOCR_Test -B build/capture-host -DABYSSAL_CAPTURE_HOST_TESTS=ON
cmake --build build/capture-host
ctest --test-dir build/capture-host --output-on-failure
```

Capture firmware built for the original Pico. Transport/BMP, VGA nibble-order
and malformed-frame rejection checks passed. A small PIO instruction simulator
checks first-clock boundary conditions, blanking clocks, luma/chroma separation,
nibble packing and stopping at the first frame boundary. It does not model
electrical timing, input synchronizers, DMA or FIFO stalls.

The owner's hardware run repeatedly identified PID=76, VER=73 and manufacturer
7FA2. Earlier capture transferred a readable internal bar pattern and a blurred,
vertically repeated scene. The replacement sampler, frame-boundary checks and
new VGA mode require hardware retesting. Focus/exposure and handwriting OCR
remain unresolved; no end-to-end OCR success is claimed.

## Repeat capture synchronization and focus checks

After one successful 640x477 save, the owner observed 183-row and 74-row
captures with matching byte counts. These remain failures; the short-window
allowance was not expanded. Startup now waits for VSYNC LOW then HIGH, each
stable for 50 us, before arming PIO while the qualified HIGH is present. End
of frame also requires HIGH stable for 50 us. Shorter HIGH transitions are
counted and ignored. The diagnostic retries up to three fresh frames, within
five seconds each, reporting `RETRY` and then final `ERROR` if none passes.
This is robustness against brief sync transitions, not proof of their physical
cause. Wiring integrity and actual waveforms remain unmeasured. A sustained
false signal may still fail capture; geometry checks remain active.

The saved 640x477 file was visually inspected: the earlier vertical duplication
was absent, but substantial blur and 16-level banding remained. VGA's 16 shades
are a memory trade-off, not a focus setting. Additional pixels or sharpening
cannot restore detail lost to defocus.

An explicit short-exposure option helps separate motion blur from defocus:

```powershell
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -Resolution VGA -Exposure Short -Output build/camera-focus.bmp
```

It disables automatic gain/exposure, sets unity gain and exposure to 16 sensor
line periods using COM8/GAIN/VREF/AECHH/AECH/COM1, preserving other window bits.
The time in seconds depends on sensor timing; no measured shutter time is
claimed. Use bright visible lighting, UV off, and a stationary high-contrast
printed target. This setting can make a dim scene too dark. `-Exposure Auto`
(the default) restores automatic gain/exposure. It is not autofocus, deblurring,
or a handwriting model. If a static well-lit target stays blurry, check lens
cleanliness/protective film and physical focus at the intended working distance.
Only adjust a free threaded lens barrel; do not force glued or fixed lenses.

Sync-filter boundary, spike/reset and timer-wrap checks passed on the host.
The updated firmware and receiver script build/parse successfully. Real-world
repeat-capture stability and optical improvement require another bench run.

## Byte-phase check before concluding the lens is at fault

The latest code review found that the early datasheet reference and revision
1.4 disagree about the U/V swap bit in COM13. The mask is now bit 0, as in
[revision 1.4, Table 5](https://web.mit.edu/6.111/www/f2016/tools/OV7670_2006.pdf).
This swaps U/V order, not Y placement, and is not by itself a demonstrated
cause of scene blur. VGA's intentional four-bit quantization does explain
banding. The minimal ISP setup has not been established as an image-quality
reference; readout success does not establish correctly focused optical detail.

A new `-ByteCheck` test preserves both byte phases of QVGA YUV from the same
frame, then sends a 640-wide comparison: left 320 columns contain the first
byte of each pixel pair, right 320 contain the second. Both retain eight bits.
This is intentionally two panels, not a vertically stacked or higher-resolution
image. With correctly aligned YUV output, one contains luma detail and the
other alternating chroma information. Compare the panels to test the earlier
phase assumption before adjusting optics. This is a diagnostic, not a claim
that arbitrary scene blur can be fixed in software.

```powershell
& .\tests\CameraOCR_Test\Save-Frame.ps1 -Port COM3 -ByteCheck -Output build/camera-bytecheck.bmp
```

It uses visible lighting and automatic exposure. No UV or short exposure is
allowed with this test. The PIO sampler reads every byte and swaps into the
same state machine only while disabled; only one program is loaded at a time
to avoid exceeding PIO instruction memory. Geometry and transfer checks remain
active. The simulator also tests the compiled all-byte sampler; host geometry
and parser tests include the diagnostic payload. A physical comparison image
is still required before assigning the remaining blur to software or optics.
