# Vendored PicoCamera

- Source: https://github.com/umeiko/PicoCamera
- Commit: `281d9ff6de5d5f6bd68c2ce75f245cc9cbadb0ea`
- Version in header: 0.4.2
- License: [MIT](LICENSE), copyright notices retained in every source file.
- Copied unmodified: `src/`, `CMakeLists.txt`, `LICENSE`, `readme.md`.
- The experimental capture-lifecycle patch has been reverted at the owner's
  request. Current library source matches this upstream commit again.
- No Arduino core or handwritten capture backend is used by the new test.
- API/example reviewed: `src/pico_camera.h`, `src/sensor.h`,
  `examples/pico_sdk_capture/main.c`, its CMake file, `docs/user_guide.md`,
  `src/driver/sccb.cpp`, `src/driver/pio_capture.cpp`, `src/sensors/ov7670.cpp`.

The wrapper and host tools live in `../../picocamera/`. Local changes to the
library have been reverted. Updating this snapshot requires reviewing
the API and repeating build/transport checks; hardware success is separate.
