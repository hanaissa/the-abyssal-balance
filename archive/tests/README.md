# Historical test implementations

These files are excluded from active submission targets. Useful current
regressions and OCR fixtures remain at their original locations under tests/.

- `WaterLevel_Test/main.c`: redundant serial-only water demonstration. The active
  WaterLevel_Test uses the unchanged LCD/RFID demo in tests/WaterLevel_Test/lcd_demo.c.
- `Integration_Test/main.c`: empty placeholder.
- `CameraOCR_Test/diagnostic_main.c`: old sensor identification diagnostic.
- `CameraOCR_Test/capture.c`, PIO, geometry/sync helpers, protocol tools and checks:
  obsolete custom capture experiments, preserved together with their README and
  historical CMake configuration.
- `CameraOCR_Test/picocamera/`: redundant compatibility entry points for the
  maintained tools/camera preview. The archive's legacy target.cmake still
  references the maintained preview; it is not an additional implementation.

Historical documentation describes earlier hardware states and superseded
commands. Use tests/README.md for current submission builds, the regression
README in tests/CameraOCR_Test/picocamera for software checks, and tools/camera
for maintained preview commands. Archive-relative implementation/dependency
paths have been adjusted where needed; old build-output paths in historical
notes are not the submission outputs.
