# Camera, OCR and LCD hardware demonstration

`main.c` is an exact copy of `src/main.c` at the submission cleanup. It keeps
the existing Camera -> OCR -> LCD behavior, initialization, single-line OCR
configuration, display caching, 500 ms delay and error handling unchanged.
It uses the production drivers and existing pin configuration.

From a root build configured with `ABYSSAL_USE_PICO_SDK=ON`:

```powershell
cmake --build build/milestone2-pico --target CameraOCR_Test
```

Flash `build/milestone2-pico/tests/CameraOCR_Test.uf2`. The LCD shows
`Code Captured:` and the recognized text, or `Read failed`. OCR remains
experimental; see [the model limits](../../drivers/camera/ocr/MODEL.md).

The maintained USB image preview is unchanged in [tools/camera](../../tools/camera/README.md).
Useful host regressions remain in `picocamera/`; obsolete diagnostics and
custom capture experiments are preserved in [the archive](../../archive/tests/README.md).
