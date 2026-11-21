# OV7670 camera driver

The working native **320x240 RGB565** implementation is in `Camera.c`, with
the checked sensor settings in `CameraSetup.h`, frame validation in
`FrameContract.h`, and the patched MIT-licensed PicoCamera dependency in
`vendor/PicoCamera/`. None of these files depends on `tests/`.

The root build links `abyssal_camera` into `abyssal_core`. On Pico it uses the
existing AppConfig.h pins, a shared 100 kHz I2C1 bus, 10 MHz requested XCLK,
normal PCLK, zero scaler delay, and a 480-source-row window. It initializes
no pumps, UV, LCD, RFID or buttons and prints nothing. It requires no computer.
RESET/PWDN remain externally controlled as documented in the pin map.

## Application API

```c
#include "Camera.h"

CameraFrame frame;
if (camera_init() == APP_OK &&
    camera_capture() == APP_OK &&
    camera_get_frame(&frame) == APP_OK) {
    /* Read frame.data: frame.length == 153600 bytes, 320x240 RGB565.
       The pointer is read-only and belongs to the driver. */
    camera_release_frame();
}
/* On shutdown: */
camera_deinit();
```

Use one control loop. Initialization and capture block with bounded hardware
waits; initialization includes a one-second settling interval. Capture rejects
a short first frame, invalid metadata and multiple observed frame starts.
The view becomes invalid on release, the next capture (including a failed
capture), a color-bar mode change or deinitialization. Reinitialization while
ready is harmless. Host builds report unsupported hardware; they do not
fabricate image data. `Camera.h` documents status codes and diagnostics.

Resources: one 153600-byte heap frame allocation (plus alignment/library
overhead), two PIO state machines, 19 PIO instruction words, PIO IRQ flag 0
in the selected block, one DMA channel and the existing XCLK PWM slice.
The driver uses a scoped raw GPIO IRQ handler instead of replacing the
application's shared GPIO callback. Camera deinit preserves the shared I2C bus.

## Optional USB preview

The preview and receiver now live in `tools/camera/`, independently of tests.
From a configured Pico root build:

```powershell
cmake -S . -B build/pico-check -DABYSSAL_BUILD_CAMERA_PREVIEW=ON
cmake --build build/pico-check --target Camera_Preview
& .\.venv-camera\Scripts\python.exe tools/camera/receive.py --port COM3 --output build/camera.png
```

Flash `build/pico-check/Camera_Preview.uf2` before receiving. Keep pump supply
off for camera-only verification. The receiver accepts `--no-display` and
`--color-bars`. Reconnect USB normally between runs. A separate standalone
SDK project is also available at `tools/camera/CMakeLists.txt`.

## Evidence and limits

After migration the physical Pico passed two captures back-to-back through
the public driver API with one initialization: 153600 bytes, one frame start,
240 observed HREF rows, and a CRC-verified received image. The preview also
checked capture-before-init, NULL/empty-frame rejection, idempotent init and
frame release. Low-level host tests cover short frames, timeout cleanup and
retry; receiver tests cover corrupt and truncated images.

Capture works. `OCR.c` now contains an **experimental** standalone recognizer
for separated handwritten A-Z/0-9, spaces and colons. Its low full-line
acceptance means it is not ready for reliable station reward decisions. See
[model evaluation and limits](ocr/MODEL.md). On-device OCR latency and real
camera/UV accuracy remain unverified; the Pico was disconnected during OCR work.

```c
char text[256];
ocr_init(); /* include OCR.h */
if (camera_capture() == APP_OK && ocr_read_text(text, sizeof text) == APP_OK) {
    /* text is uppercase; still validate against the intended application. */
}
```

`ocr_configure()` selects clockwise rotation, an optional ROI after rotation,
and dark-ink versus bright-UV polarity. It does not turn the UV lamp on.
`ocr_read_text()` reads the current captured frame; it does not capture or
release it. `ocr_read_frame()` runs the same C pipeline on a supplied frame.
Both clear output on failure and return `APP_NOT_READY` for unreadable input.
