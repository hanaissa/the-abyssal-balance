# Camera preview tools

This folder is independent of `tests/`. The actual sensor configuration,
capture, frame validation and resource ownership are in
[`drivers/camera`](../../drivers/camera/README.md).

`main.c` builds as `Camera_Preview` from the root with
`ABYSSAL_BUILD_CAMERA_PREVIEW=ON`, or as a standalone Pico SDK project using
this folder's CMakeLists.txt. It calls the public driver API, checks boundary
cases and two consecutive captures, then sends one CRC-protected frame over
USB. No other hardware driver is initialized.

After flashing the preview, run from the repository root:

```powershell
& .\.venv-camera\Scripts\python.exe tools/camera/receive.py --port COM3 --output build/camera.png
```

Use `--no-display` to save without opening a window or `--color-bars` for the
internal DSP pattern. Raw `.rgb565` sidecars preserve the received payload.
Python dependencies are in `requirements.txt`. Reconnect Pico USB normally
before another run; this preview performs one sequence per boot.

New scene previews also run the experimental **on-Pico** OCR on the captured
frame, printing `OCR_STATUS` and `OCR_TEXT` only for an accepted result. This
does not use Python OCR. The receiver allows up to 120 seconds for the full
sequence; physical inference latency has not yet been measured. Read the
[OCR limits](../../drivers/camera/ocr/MODEL.md) before relying on its text.
